/* Loadout Optimization (Phase 9) */
#include "core/loadout_optimizer.h"
#include "core/app_state.h"
#include "core/equipment/equipment.h"
#include "core/equipment/equipment_perf.h" /* arena + profiler (Phase 14) */
#include "core/equipment/equipment_stats.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/stat_cache.h"
#include "core\inventory\inventory.h"
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

/* Simple FNV-1a 32-bit */
static unsigned int fnv1a(const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*) data;
    unsigned int h = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

int rogue_loadout_snapshot(RogueLoadoutSnapshot* out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof *out);
    out->slot_count = ROGUE_EQUIP_SLOT_COUNT;
    for (int i = 0; i < ROGUE_EQUIP_SLOT_COUNT; i++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) i);
        out->inst_indices[i] = inst;
        if (inst >= 0)
        {
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            out->def_indices[i] = it ? it->def_index : -1;
        }
        else
        {
            out->def_indices[i] = -1;
        }
    }
    out->dps_estimate = g_player_stat_cache.dps_estimate;
    out->ehp_estimate = g_player_stat_cache.ehp_estimate;
    out->mobility_index = g_player_stat_cache.mobility_index;
    return 0;
}

int rogue_loadout_compare(const RogueLoadoutSnapshot* a, const RogueLoadoutSnapshot* b,
                          int* out_slot_changed)
{
    if (!a || !b)
        return -1;
    int diffs = 0;
    int n = (a->slot_count < b->slot_count) ? a->slot_count : b->slot_count;
    for (int i = 0; i < n; i++)
    {
        int changed = (a->def_indices[i] != b->def_indices[i]);
        if (changed)
            diffs++;
        if (out_slot_changed)
            out_slot_changed[i] = changed;
    }
    return diffs;
}

unsigned int rogue_loadout_hash(const RogueLoadoutSnapshot* s)
{
    if (!s)
        return 0;
    unsigned int h = 0;
    h ^= fnv1a(s->def_indices, sizeof(int) * ROGUE_EQUIP_SLOT_COUNT);
    h = (h << 1) ^ fnv1a(&s->dps_estimate, sizeof(int) * 3);
    return h;
}

/* Evaluated cache (simple open addressing) */
typedef struct CacheEntry
{
    unsigned int hash;
    int used;
} CacheEntry;
static CacheEntry g_cache[256];
void rogue_loadout_cache_reset(void) { memset(g_cache, 0, sizeof g_cache); }
static int cache_contains(unsigned int h)
{
    unsigned int idx = h & 255u;
    for (int i = 0; i < 256; i++)
    {
        unsigned int p = (idx + i) & 255u;
        if (!g_cache[p].used)
            return 0;
        if (g_cache[p].hash == h)
            return 1;
    }
    return 0;
}
static void cache_insert(unsigned int h)
{
    unsigned int idx = h & 255u;
    for (int i = 0; i < 256; i++)
    {
        unsigned int p = (idx + i) & 255u;
        if (!g_cache[p].used)
        {
            g_cache[p].used = 1;
            g_cache[p].hash = h;
            return;
        }
        if (g_cache[p].hash == h)
            return;
    }
}

static int g_cache_hits = 0, g_cache_inserts = 0;
void rogue_loadout_cache_stats(int* used, int* capacity, int* hits, int* inserts)
{
    if (used)
    {
        int c = 0;
        for (int i = 0; i < 256; i++)
            if (g_cache[i].used)
                c++;
        *used = c;
    }
    if (capacity)
        *capacity = 256;
    if (hits)
        *hits = g_cache_hits;
    if (inserts)
        *inserts = g_cache_inserts;
}

/* Helper: recompute stat cache after modifying equipment */
static void recompute_stats(void)
{
    extern RoguePlayer g_exposed_player_for_stats;
    rogue_stat_cache_mark_dirty();
    rogue_equipment_apply_stat_bonuses(&g_exposed_player_for_stats);
    rogue_stat_cache_force_update(&g_exposed_player_for_stats);
}

/* Attempt equipping instance (if category fits) returning 0 on success else negative. Wraps
 * existing API. */
static int try_equip_slot(enum RogueEquipSlot slot, int inst_index, int* prev_out)
{
    if (prev_out)
        *prev_out = rogue_equip_get(slot);
    return rogue_equip_try(slot, inst_index);
}

/* Evaluate current equipped DPS/EHP/Mobility after ensuring cache updated */
static void ensure_stats(void)
{
    if (g_player_stat_cache.dirty)
    {
        recompute_stats();
    }
}

/* Return 1 if constraints satisfied, 0 otherwise */
static int constraints_ok(int min_mobility, int min_ehp)
{
    ensure_stats();
    if (g_player_stat_cache.mobility_index < min_mobility)
        return 0;
    if (g_player_stat_cache.ehp_estimate < min_ehp)
        return 0;
    return 1;
}

/* Collect candidate item instances by scanning active instances and filtering by category vs slot.
 */
static int collect_candidates(enum RogueEquipSlot slot, int* out_indices, int cap)
{
    int count = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP && count < cap; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if (!d)
            continue;
        int want_cat = 0;
        switch (slot)
        {
        case ROGUE_EQUIP_WEAPON:
            want_cat = ROGUE_ITEM_WEAPON;
            break;
        case ROGUE_EQUIP_OFFHAND:
            want_cat = ROGUE_ITEM_ARMOR;
            break;
        default:
            want_cat = ROGUE_ITEM_ARMOR;
            break;
        }
        if (d->category != want_cat)
            continue;
        out_indices[count++] = i;
    }
    return count;
}

/* Hill-climb: single pass trying improving swaps per slot until no improvement. */
int rogue_loadout_optimize(int min_mobility, int min_ehp)
{
    rogue_equip_profiler_zone_begin("optimize");
    ensure_stats();
    RogueLoadoutSnapshot baseline;
    rogue_loadout_snapshot(&baseline);
    unsigned int base_hash = rogue_loadout_hash(&baseline);
    cache_insert(base_hash);
    g_cache_inserts++;
    int improved_total = 0;
    int progress = 1;
    int guard = 0;
    while (progress && guard < 32)
    {
        progress = 0;
        guard++;
        for (int slot = 0; slot < ROGUE_EQUIP_SLOT_COUNT; ++slot)
        {
            int current_inst = rogue_equip_get((enum RogueEquipSlot) slot);
            int* candidates = (int*) rogue_equip_frame_alloc(sizeof(int) * 128, sizeof(int));
            if (!candidates)
            {
                int candidates_stack[128];
                candidates = candidates_stack;
            }
            int ccount = collect_candidates((enum RogueEquipSlot) slot, candidates, 128);
            int best_dps = g_player_stat_cache.dps_estimate;
            int best_inst = current_inst;
            for (int ci = 0; ci < ccount; ++ci)
            {
                int cand = candidates[ci];
                if (cand == current_inst)
                    continue;
                int prev;
                if (try_equip_slot((enum RogueEquipSlot) slot, cand, &prev) == 0)
                {
                    recompute_stats();
                    RogueLoadoutSnapshot snap;
                    rogue_loadout_snapshot(&snap);
                    unsigned int h = rogue_loadout_hash(&snap);
                    if (cache_contains(h))
                    {
                        g_cache_hits++; /* revert */
                        rogue_equip_try((enum RogueEquipSlot) slot, prev);
                        continue;
                    }
                    else
                    {
                        cache_insert(h);
                        g_cache_inserts++;
                    }
                    if (constraints_ok(min_mobility, min_ehp))
                    {
                        if (g_player_stat_cache.dps_estimate > best_dps)
                        {
                            best_dps = g_player_stat_cache.dps_estimate;
                            best_inst = cand;
                        }
                    }
                    rogue_equip_try((enum RogueEquipSlot) slot, prev);
                    recompute_stats();
                }
            }
            if (best_inst != current_inst)
            {
                rogue_equip_try((enum RogueEquipSlot) slot, best_inst);
                recompute_stats();
                improved_total++;
                progress = 1;
            }
        }
    }
    rogue_equip_profiler_zone_end("optimize");
    return improved_total;
}

/* ---------------- Phase 14.4: Async optimization ---------------- */
static volatile int g_async_running = 0;
static int g_async_min_mob = 0, g_async_min_ehp = 0;
static int g_async_result = 0;
#if defined(_WIN32)
static HANDLE g_async_thread = NULL;
static DWORD WINAPI rogue__opt_thread(LPVOID p)
{
    (void) p;
    g_async_result = rogue_loadout_optimize(g_async_min_mob, g_async_min_ehp);
    g_async_running = 0;
    return 0;
}
#endif
int rogue_loadout_optimize_async(int min_mobility, int min_ehp)
{
    if (g_async_running)
        return -1;
    g_async_running = 1;
    g_async_min_mob = min_mobility;
    g_async_min_ehp = min_ehp;
    g_async_result = 0;
    rogue_equip_profiler_zone_begin("optimize_async_launch");
    rogue_equip_profiler_zone_end("optimize_async_launch");
#if defined(_WIN32)
    g_async_thread = CreateThread(NULL, 0, rogue__opt_thread, NULL, 0, NULL);
    if (!g_async_thread)
    {
        g_async_running = 0;
        return -2;
    }
    return 0;
#else
    /* Fallback: run synchronously (still sets result) */
    g_async_result = rogue_loadout_optimize(min_mobility, min_ehp);
    g_async_running = 0;
    return 0;
#endif
}
int rogue_loadout_optimize_join(void)
{
    if (!g_async_running)
    {
#if defined(_WIN32)
        if (g_async_thread)
        {
            WaitForSingleObject(g_async_thread, INFINITE);
            CloseHandle(g_async_thread);
            g_async_thread = NULL;
            return g_async_result;
        }
#endif
        return -1;
    }
#if defined(_WIN32)
    WaitForSingleObject(g_async_thread, INFINITE);
    CloseHandle(g_async_thread);
    g_async_thread = NULL;
    int res = g_async_result;
    g_async_running = 0;
    return res;
#else
    return g_async_result; /* already done in fallback */
#endif
}
int rogue_loadout_optimize_async_running(void) { return g_async_running ? 1 : 0; }
