/* item_collision_cache.c - Milestone 1.2 initial implementation
 * A fixed-capacity LRU cache of RoguePixelMaskSet keyed by RogueItemDefHandle.
 * Simplifications in this slice:
 *   - No background thread loading (future: async build integration once weapon masks generalized)
 *   - Memory usage approximated (bit buffers + compressed + distance field + mipmaps)
 *   - Eviction policy: simple LRU when capacity OR memory limit exceeded
 *   - Asset timestamp/mtime tracking deferred; generation snapshot guards stale handles
 */
#include "item_collision_cache.h"
#include "util/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <sys/stat.h>
#else
#include <sys/stat.h>
#endif
#include "core/integration/resource_lock.h"
#include "core/integration/thread_pool.h"

/* Forward declarations for existing ensure path (weapon-centric). In a future slice this will map
   generic item sprite -> pixel masks; for now we reuse weapon id path if category == WEAPON and
   fall back to a placeholder for others. */
RogueHitPixelMaskSet* rogue_hit_pixel_masks_ensure(int weapon_id);

static struct
{
    int initialized;
    RogueItemCollisionCacheEntry entries[ROGUE_COLLISION_CACHE_SIZE];
    RogueItemCollisionCacheEntry* lru_head; /* MRU at head */
    RogueItemCollisionCacheEntry* lru_tail; /* LRU at tail */
    uint64_t access_counter;
    RogueItemCollisionCacheStats stats;
    RogueRwLock* lock;          /* simple RW lock placeholder (future reader sharing) */
    struct RogueThreadPool* tp; /* optional background worker pool */
    int prefetch_enabled;
    int prefetch_budget;
} g_cache;

static uint64_t file_mtime_simple(const char* path)
{
    if (!path || !*path)
        return 0;
#ifdef _WIN32
    struct _stat s;
    if (_stat(path, &s) == 0)
        return (uint64_t) s.st_mtime;
#else
    struct stat s;
    if (stat(path, &s) == 0)
        return (uint64_t) s.st_mtime;
#endif
    return 0;
}

static uint64_t (*g_mtime_hook)(const char* path) = NULL;

static void lru_move_front(RogueItemCollisionCacheEntry* e)
{
    if (g_cache.lru_head == e)
        return;
    /* unlink */
    if (e->prev)
        e->prev->next = e->next;
    if (e->next)
        e->next->prev = e->prev;
    if (g_cache.lru_tail == e)
        g_cache.lru_tail = e->prev;
    /* insert at head */
    e->prev = NULL;
    e->next = g_cache.lru_head;
    if (g_cache.lru_head)
        g_cache.lru_head->prev = e;
    g_cache.lru_head = e;
    if (!g_cache.lru_tail)
        g_cache.lru_tail = e;
}

static size_t approx_mask_bytes(const RogueHitPixelMaskFrame* f)
{
    if (!f)
        return 0;
    size_t total = (size_t) f->pitch_words * (size_t) f->height * sizeof(uint32_t);
    if (f->compressed)
        total += f->compressed_size;
    if (f->distance_field)
        total += (size_t) f->width * (size_t) f->height * sizeof(int16_t);
    for (int i = 0; i < f->mipmap_count - 1 && f->mipmaps; ++i)
    {
        const RogueHitPixelMaskMipmapLevel* ml = &f->mipmaps[i];
        if (ml->bits)
            total += (size_t) ml->pitch_words * (size_t) ml->height * sizeof(uint32_t);
    }
    return total;
}

static size_t approx_set_bytes(const RoguePixelMaskSet* set)
{
    if (!set)
        return 0;
    size_t s = sizeof(*set);
    for (int i = 0; i < set->frame_count; ++i)
        s += approx_mask_bytes(&set->frames[i]);
    return s;
}

void rogue_item_collision_cache_init(void)
{
    if (g_cache.initialized)
        return;
    memset(&g_cache, 0, sizeof(g_cache));
    g_cache.initialized = 1;
    g_cache.lock = rogue_rwlock_create(1200, "item_collision_cache");
    g_cache.prefetch_enabled = 1;
    g_cache.prefetch_budget = 2;
}

static void free_entry(RogueItemCollisionCacheEntry* e)
{
    if (!e || !e->mask_set)
        return;
    /* Reuse existing reset API if available; otherwise free frames manually. */
    for (int i = 0; i < e->mask_set->frame_count; ++i)
    {
        RogueHitPixelMaskFrame* f = &e->mask_set->frames[i];
        free(f->bits);
        if (f->compressed)
            free(f->compressed);
        if (f->distance_field)
            free(f->distance_field);
        if (f->mipmaps)
        {
            for (int m = 0; m < f->mipmap_count - 1; ++m)
            {
                if (f->mipmaps[m].bits)
                    free(f->mipmaps[m].bits);
            }
            free(f->mipmaps);
        }
        memset(f, 0, sizeof(*f));
    }
    free(e->mask_set);
    e->mask_set = NULL;
}

void rogue_item_collision_cache_reset(void)
{
    if (!g_cache.initialized)
        return;
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    RogueItemCollisionCacheEntry* it = g_cache.lru_head;
    while (it)
    {
        free_entry(it);
        it = it->next;
    }
    memset(&g_cache, 0, sizeof(g_cache));
    g_cache.initialized = 1;
    g_cache.lock = rogue_rwlock_create(1200, "item_collision_cache");
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
}

static void enforce_memory_limit(void)
{
    size_t limit_bytes = (size_t) ROGUE_COLLISION_CACHE_MAX_MEMORY_MB * 1024ULL * 1024ULL;
    while (g_cache.stats.approx_bytes > limit_bytes && g_cache.lru_tail)
    {
        RogueItemCollisionCacheEntry* victim = g_cache.lru_tail;
        if (!victim)
            break;
        RogueItemCollisionCacheEntry* prev = victim->prev;
        /* unlink victim */
        if (victim->prev)
            victim->prev->next = NULL;
        g_cache.lru_tail = victim->prev;
        if (g_cache.lru_head == victim)
            g_cache.lru_head = NULL;
        g_cache.stats.approx_bytes -= approx_set_bytes(victim->mask_set);
        free_entry(victim);
        memset(victim, 0, sizeof(*victim));
        g_cache.stats.evictions++;
        victim->prev = victim->next = NULL;
        victim = prev;
    }
}

static RogueItemCollisionCacheEntry* find_or_allocate_slot(RogueItemDefHandle h, uint32_t gen)
{
    /* search existing */
    for (RogueItemCollisionCacheEntry* it = g_cache.lru_head; it; it = it->next)
    {
        if (it->handle == h && it->generation_snapshot == gen)
            return it;
    }
    /* find free */
    for (int i = 0; i < ROGUE_COLLISION_CACHE_SIZE; ++i)
    {
        if (g_cache.entries[i].handle == 0 && g_cache.entries[i].mask_set == NULL)
        {
            return &g_cache.entries[i];
        }
    }
    /* evict LRU tail */
    RogueItemCollisionCacheEntry* victim = g_cache.lru_tail;
    if (victim)
    {
        /* unlink from LRU */
        if (victim->prev)
            victim->prev->next = NULL;
        g_cache.lru_tail = victim->prev;
        if (g_cache.lru_head == victim)
            g_cache.lru_head = NULL;
        g_cache.stats.approx_bytes -= approx_set_bytes(victim->mask_set);
        free_entry(victim);
        memset(victim, 0, sizeof(*victim));
        g_cache.stats.evictions++;
    }
    return victim; /* may be NULL if size==0 */
}

RoguePixelMaskSet* rogue_item_collision_cache_get(RogueItemDefHandle handle,
                                                  const RoguePixelMaskLoadConfig* cfg)
{
    if (!g_cache.initialized)
        rogue_item_collision_cache_init();
    /* cfg currently unused in this slice (future: control distance field / mipmaps). */
    (void) cfg;
    /* Prefetch bookkeeping: gather candidate handles under lock, queue after releasing. */
    RogueItemDefHandle prefetch_list[16];
    int prefetch_count = 0;
    RoguePixelMaskLoadConfig prefetch_cfg = rogue_pixel_mask_load_config_default();
    /* Try read lock first for fast hit path; upgrade to write on miss. */
    if (g_cache.lock)
        rogue_rwlock_acquire_read(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    g_cache.stats.lookups++;
    if (handle == ROGUE_ITEM_DEF_INVALID_HANDLE)
    {
        g_cache.stats.misses++;
        if (g_cache.lock)
            rogue_rwlock_release_read(g_cache.lock);
        return NULL;
    }
    int index = rogue_item_def_index_from_handle(handle);
    if (index < 0)
    {
        g_cache.stats.misses++;
        if (g_cache.lock)
            rogue_rwlock_release_read(g_cache.lock);
        return NULL; /* stale or invalid */
    }
    /* Derive generation bits from handle (upper 16 bits) */
    uint32_t gen = (uint32_t) (handle >> 16);
    RogueItemCollisionCacheEntry* e = find_or_allocate_slot(handle, gen);
    if (!e)
    {
        g_cache.stats.misses++;
        if (g_cache.lock)
            rogue_rwlock_release_read(g_cache.lock);
        return NULL;
    }
    if (e->handle == handle && e->mask_set)
    {
        /* hit */
        g_cache.stats.hits++;
        e->last_access_tick = ++g_cache.access_counter;
        e->access_count++;
        lru_move_front(e);
        RoguePixelMaskSet* out = e->mask_set;
        if (g_cache.lock)
            rogue_rwlock_release_read(g_cache.lock);
        return out;
    }
    /* miss -> upgrade to write for build */
    if (g_cache.lock)
    {
        rogue_rwlock_release_read(g_cache.lock);
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    }
    /* Re-check after upgrading in case another thread filled it */
    e = find_or_allocate_slot(handle, gen);
    if (e && e->handle == handle && e->mask_set)
    {
        g_cache.stats.hits++;
        e->last_access_tick = ++g_cache.access_counter;
        e->access_count++;
        lru_move_front(e);
        RoguePixelMaskSet* out2 = e->mask_set;
        if (g_cache.lock)
            rogue_rwlock_release_write(g_cache.lock);
        return out2;
    }
    /* miss -> build */
    g_cache.stats.misses++;
    e->handle = handle;
    e->generation_snapshot = gen;
    e->last_access_tick = ++g_cache.access_counter;
    e->access_count = 1;
    e->mask_set = (RoguePixelMaskSet*) calloc(1, sizeof(RoguePixelMaskSet));
    if (!e->mask_set)
    {
        memset(e, 0, sizeof(*e));
        return NULL;
    }
    /* For now only weapon category builds real mask set (reuse ensure). Others left empty. */
    const RogueItemDef* def = rogue_item_def_at(index);
    if (def && def->category == ROGUE_ITEM_WEAPON)
    {
        /* Weapon id for ensure path is currently index; future slice may map per-sprite file. */
        RogueHitPixelMaskSet* weapon_set = rogue_hit_pixel_masks_ensure(index);
        if (weapon_set)
        {
            e->mask_set->frame_count = weapon_set->frame_count;
            e->mask_set->weapon_id = index;
            e->mask_set->ready = weapon_set->ready;
            for (int i = 0; i < weapon_set->frame_count; ++i)
            {
                RogueHitPixelMaskFrame* dst = &e->mask_set->frames[i];
                RogueHitPixelMaskFrame* src = &weapon_set->frames[i];
                *dst = *src; /* copy POD members first */
                /* Deep copy dynamic buffers */
                if (src->bits && src->pitch_words > 0 && src->height > 0)
                {
                    size_t bits_sz =
                        (size_t) src->pitch_words * (size_t) src->height * sizeof(uint32_t);
                    dst->bits = (uint32_t*) malloc(bits_sz);
                    if (dst->bits)
                        memcpy(dst->bits, src->bits, bits_sz);
                }
                if (src->compressed && src->compressed_size)
                {
                    dst->compressed = malloc(src->compressed_size);
                    if (dst->compressed)
                        memcpy(dst->compressed, src->compressed, src->compressed_size);
                }
                if (src->distance_field && src->width > 0 && src->height > 0)
                {
                    size_t df_sz = (size_t) src->width * (size_t) src->height * sizeof(int16_t);
                    dst->distance_field = (int16_t*) malloc(df_sz);
                    if (dst->distance_field)
                        memcpy(dst->distance_field, src->distance_field, df_sz);
                }
                if (src->mipmaps && src->mipmap_count > 1)
                {
                    int levels = src->mipmap_count - 1;
                    dst->mipmaps = (RogueHitPixelMaskMipmapLevel*) calloc(
                        levels, sizeof(RogueHitPixelMaskMipmapLevel));
                    if (dst->mipmaps)
                    {
                        for (int m = 0; m < levels; ++m)
                        {
                            dst->mipmaps[m] = src->mipmaps[m];
                            if (src->mipmaps[m].bits && src->mipmaps[m].pitch_words > 0 &&
                                src->mipmaps[m].height > 0)
                            {
                                size_t ml_sz = (size_t) src->mipmaps[m].pitch_words *
                                               (size_t) src->mipmaps[m].height * sizeof(uint32_t);
                                dst->mipmaps[m].bits = (uint32_t*) malloc(ml_sz);
                                if (dst->mipmaps[m].bits)
                                    memcpy(dst->mipmaps[m].bits, src->mipmaps[m].bits, ml_sz);
                            }
                        }
                    }
                }
            }
            /* Attempt naive timestamp: derive a plausible sprite path if defined (sprite_sheet
             * field). */
            if (def->sprite_sheet[0])
            {
                /* Assume assets/<sprite_sheet> if not already containing a path */
                char path[256];
                if (strchr(def->sprite_sheet, '/') || strchr(def->sprite_sheet, '\\'))
                    snprintf(path, sizeof(path), "%s", def->sprite_sheet);
                else
                    snprintf(path, sizeof(path), "assets/%s", def->sprite_sheet);
                /* Use hook if provided for deterministic tests; else filesystem mtime. */
                e->asset_timestamp = g_mtime_hook ? g_mtime_hook(path) : file_mtime_simple(path);

                /* Prefetch related items sharing the same sprite sheet: collect under lock,
                   submit after releasing the lock to avoid nested locking. */
                if (g_cache.prefetch_enabled && g_cache.tp)
                {
                    int total = rogue_item_defs_count();
                    for (int i2 = index + 1; i2 < total && prefetch_count < g_cache.prefetch_budget;
                         ++i2)
                    {
                        const RogueItemDef* d2 = rogue_item_def_at(i2);
                        if (!d2 || d2->category != ROGUE_ITEM_WEAPON)
                            continue;
                        if (strcmp(d2->sprite_sheet, def->sprite_sheet) != 0)
                            continue;
                        prefetch_list[prefetch_count++] = rogue_item_def_handle_from_index(i2);
                    }
                }
            }
        }
    }
    /* Track approximate bytes */
    g_cache.stats.approx_bytes += approx_set_bytes(e->mask_set);
    lru_move_front(e);
    enforce_memory_limit();
    RoguePixelMaskSet* out_new = e->mask_set;
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
    /* Queue prefetch jobs outside of the write lock. */
    if (prefetch_count > 0 && g_cache.prefetch_enabled && g_cache.tp)
    {
        for (int i = 0; i < prefetch_count; ++i)
        {
            RogueItemDefHandle h2 = prefetch_list[i];
            if (!rogue_item_collision_cache_is_ready(h2))
                (void) rogue_item_collision_cache_request_async(h2, &prefetch_cfg);
        }
    }
    return out_new;
}

RogueItemCollisionCacheStats rogue_item_collision_cache_get_stats(void) { return g_cache.stats; }

void rogue_item_collision_cache_invalidate_handle(RogueItemDefHandle handle)
{
    if (!g_cache.initialized || handle == ROGUE_ITEM_DEF_INVALID_HANDLE)
        return;
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    for (RogueItemCollisionCacheEntry* it = g_cache.lru_head; it; it = it->next)
    {
        if (it->handle == handle)
        {
            g_cache.stats.approx_bytes -= approx_set_bytes(it->mask_set);
            free_entry(it);
            memset(it, 0, sizeof(*it));
            g_cache.stats.invalidations++;
            break;
        }
    }
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
}

void rogue_item_collision_cache_invalidate_all(void)
{
    if (!g_cache.initialized)
        return;
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    RogueItemCollisionCacheEntry* it = g_cache.lru_head;
    while (it)
    {
        g_cache.stats.approx_bytes -= approx_set_bytes(it->mask_set);
        free_entry(it);
        memset(it, 0, sizeof(*it));
        g_cache.stats.invalidations++;
        it = it->next;
    }
    g_cache.lru_head = g_cache.lru_tail = NULL;
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
}

/* ---------- Background loading & hot-reload extensions ---------- */

void rogue_item_collision_cache_set_thread_pool(struct RogueThreadPool* tp) { g_cache.tp = tp; }

int rogue_item_collision_cache_is_ready(RogueItemDefHandle handle)
{
    if (!g_cache.initialized)
        return 0;
    if (handle == ROGUE_ITEM_DEF_INVALID_HANDLE)
        return 0;
    if (g_cache.lock)
        rogue_rwlock_acquire_read(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    /* generation must match */
    uint32_t gen = (uint32_t) (handle >> 16);
    for (RogueItemCollisionCacheEntry* it = g_cache.lru_head; it; it = it->next)
    {
        if (it->handle == handle && it->generation_snapshot == gen && it->mask_set)
        {
            if (g_cache.lock)
                rogue_rwlock_release_read(g_cache.lock);
            return 1;
        }
    }
    if (g_cache.lock)
        rogue_rwlock_release_read(g_cache.lock);
    return 0;
}

typedef struct ItemCacheAsyncJob
{
    RogueItemDefHandle handle;
    RoguePixelMaskLoadConfig cfg;
} ItemCacheAsyncJob;

static void item_cache_worker(void* user)
{
    ItemCacheAsyncJob* job = (ItemCacheAsyncJob*) user;
    /* Build synchronously into cache via public get() path */
    RoguePixelMaskSet* set = rogue_item_collision_cache_get(job->handle, &job->cfg);
    (void) set;
    free(job);
}

/* internal: request async with optional prefetch fan-out control */
static int request_async_internal(RogueItemDefHandle handle, const RoguePixelMaskLoadConfig* cfg,
                                  int allow_prefetch)
{
    if (!g_cache.initialized)
        rogue_item_collision_cache_init();
    if (handle == ROGUE_ITEM_DEF_INVALID_HANDLE)
        return 0;
    if (rogue_item_collision_cache_is_ready(handle))
        return 1;
    if (!g_cache.tp || !g_cache.tp->threads || g_cache.tp->thread_count <= 0)
    {
        /* No pool -> build now (still returns 1 on success) */
        return rogue_item_collision_cache_get(handle, cfg) != NULL;
    }
    ItemCacheAsyncJob* job = (ItemCacheAsyncJob*) malloc(sizeof(ItemCacheAsyncJob));
    if (!job)
        return 0;
    job->handle = handle;
    job->cfg = cfg ? *cfg : rogue_pixel_mask_load_config_default();
    int ok = rogue_thread_pool_submit(g_cache.tp, item_cache_worker, job);
    if (ok == 0)
    {
        /* queued */
        if (allow_prefetch && g_cache.prefetch_enabled)
        {
            /* Determine index and def to collect siblings sharing the same sheet */
            int index = rogue_item_def_index_from_handle(handle);
            const RogueItemDef* def = (index >= 0) ? rogue_item_def_at(index) : NULL;
            if (def && def->category == ROGUE_ITEM_WEAPON && def->sprite_sheet[0])
            {
                int total = rogue_item_defs_count();
                RoguePixelMaskLoadConfig cfg2 = rogue_pixel_mask_load_config_default();
                int queued = 0;
                for (int i2 = index + 1; i2 < total && queued < g_cache.prefetch_budget; ++i2)
                {
                    const RogueItemDef* d2 = rogue_item_def_at(i2);
                    if (!d2 || d2->category != ROGUE_ITEM_WEAPON)
                        continue;
                    if (strcmp(d2->sprite_sheet, def->sprite_sheet) != 0)
                        continue;
                    RogueItemDefHandle h2 = rogue_item_def_handle_from_index(i2);
                    if (!rogue_item_collision_cache_is_ready(h2))
                    {
                        (void) request_async_internal(h2, &cfg2, /*allow_prefetch=*/0);
                        queued++;
                    }
                }
            }
        }
        return 1;
    }
    free(job);
    /* Fallback: build synchronously */
    return rogue_item_collision_cache_get(handle, cfg) != NULL;
}

int rogue_item_collision_cache_request_async(RogueItemDefHandle handle,
                                             const RoguePixelMaskLoadConfig* cfg)
{
    return request_async_internal(handle, cfg, /*allow_prefetch=*/1);
}

void rogue_item_collision_cache_set_mtime_hook(uint64_t (*hook)(const char* path))
{
    g_mtime_hook = hook;
}

int rogue_item_collision_cache_poll(int max_to_check)
{
    if (!g_cache.initialized || max_to_check <= 0)
        return 0;
    int invalidated = 0;
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    RogueItemCollisionCacheEntry* it = g_cache.lru_head;
    while (it && max_to_check-- > 0)
    {
        RogueItemCollisionCacheEntry* next = it->next; /* protect iterator on invalidate */
        const RogueItemDef* def = NULL;
        int idx = rogue_item_def_index_from_handle(it->handle);
        if (idx >= 0)
            def = rogue_item_def_at(idx);
        const char* sheet = (def && def->sprite_sheet[0]) ? def->sprite_sheet : NULL;
        char path[256] = {0};
        if (sheet)
        {
            if (strchr(sheet, '/') || strchr(sheet, '\\'))
                snprintf(path, sizeof(path), "%s", sheet);
            else
                snprintf(path, sizeof(path), "assets/%s", sheet);
        }
        uint64_t now_ts = 0;
        if (path[0])
            now_ts = g_mtime_hook ? g_mtime_hook(path) : file_mtime_simple(path);
        if (now_ts != 0 && now_ts > it->asset_timestamp && it->mask_set)
        {
            /* Source changed -> invalidate */
            g_cache.stats.approx_bytes -= approx_set_bytes(it->mask_set);
            free_entry(it);
            memset(it, 0, sizeof(*it));
            g_cache.stats.invalidations++;
            invalidated++;
        }
        it = next;
    }
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
    return invalidated;
}

/* Prefetch controls */
void rogue_item_collision_cache_set_prefetch(int enabled, int budget)
{
    if (!g_cache.initialized)
        rogue_item_collision_cache_init();
    if (budget < 0)
        budget = 0;
    if (budget > 16)
        budget = 16; /* keep bounded */
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    g_cache.prefetch_enabled = enabled ? 1 : 0;
    g_cache.prefetch_budget = budget;
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
}

/* Normalize a sprite path to assets/ prefix if bare name */
static void normalize_sprite_path(const char* in, char* out, size_t out_sz)
{
    if (!in || !*in)
    {
        out[0] = '\0';
        return;
    }
    if (strchr(in, '/') || strchr(in, '\\'))
        snprintf(out, out_sz, "%s", in);
    else
        snprintf(out, out_sz, "assets/%s", in);
}

void rogue_item_collision_cache_invalidate_sprite(const char* sprite_path)
{
    if (!g_cache.initialized || !sprite_path || !*sprite_path)
        return;
    char norm[256];
    normalize_sprite_path(sprite_path, norm, sizeof(norm));
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    for (RogueItemCollisionCacheEntry* it = g_cache.lru_head; it; it = it->next)
    {
        const RogueItemDef* def = NULL;
        int idx = rogue_item_def_index_from_handle(it->handle);
        if (idx >= 0)
            def = rogue_item_def_at(idx);
        if (!def)
            continue;
        char path[256];
        normalize_sprite_path(def->sprite_sheet, path, sizeof(path));
        if (strcmp(path, norm) == 0 && it->mask_set)
        {
            g_cache.stats.approx_bytes -= approx_set_bytes(it->mask_set);
            free_entry(it);
            memset(it, 0, sizeof(*it));
            g_cache.stats.invalidations++;
        }
    }
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
}

/* Optimize/compact cache: remove tombstones, rebuild LRU deterministically, recompute bytes. */
void rogue_item_collision_cache_optimize(void)
{
    if (!g_cache.initialized)
        return;
    if (g_cache.lock)
        rogue_rwlock_acquire_write(g_cache.lock, ROGUE_LOCK_PRIORITY_NORMAL, -1);
    /* Collect alive entries into a temporary array */
    RogueItemCollisionCacheEntry* alive[ROGUE_COLLISION_CACHE_SIZE];
    int alive_count = 0;
    for (int i = 0; i < ROGUE_COLLISION_CACHE_SIZE; ++i)
    {
        RogueItemCollisionCacheEntry* e = &g_cache.entries[i];
        if (e->handle != 0 && e->mask_set != NULL)
            alive[alive_count++] = e;
        else if (e->handle == 0 && e->mask_set == NULL)
            ; /* free slot */
        else
        {
            /* Tombstone: partially filled or freed -> fully reset */
            free_entry(e);
            memset(e, 0, sizeof(*e));
        }
    }
    /* Sort alive by last_access_tick descending (MRU first); stable by index for parity */
    for (int i = 0; i < alive_count; ++i)
    {
        for (int j = i + 1; j < alive_count; ++j)
        {
            if (alive[j]->last_access_tick > alive[i]->last_access_tick)
            {
                RogueItemCollisionCacheEntry* tmp = alive[i];
                alive[i] = alive[j];
                alive[j] = tmp;
            }
        }
    }
    /* Relink LRU list */
    g_cache.lru_head = g_cache.lru_tail = NULL;
    for (int i = 0; i < alive_count; ++i)
    {
        RogueItemCollisionCacheEntry* e = alive[i];
        e->prev = NULL;
        e->next = NULL;
        if (!g_cache.lru_head)
        {
            g_cache.lru_head = g_cache.lru_tail = e;
        }
        else
        {
            e->next = g_cache.lru_head;
            g_cache.lru_head->prev = e;
            g_cache.lru_head = e; /* maintain MRU at head */
        }
    }
    /* Recompute approx bytes */
    size_t bytes = 0;
    for (int i = 0; i < alive_count; ++i)
        bytes += approx_set_bytes(alive[i]->mask_set);
    g_cache.stats.approx_bytes = bytes;
    /* Enforce memory limit after compaction */
    enforce_memory_limit();
    if (g_cache.lock)
        rogue_rwlock_release_write(g_cache.lock);
}
