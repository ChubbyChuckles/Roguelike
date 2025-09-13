/* item_collision_cache.h - Milestone 1.2: Item -> Pixel Mask collision cache (initial slice)
 * Provides a lightweight LRU cache mapping RogueItemDefHandle to RogueHitPixelMaskSet.
 * This initial implementation focuses on:
 *   - Fixed-capacity LRU list (ROGUE_COLLISION_CACHE_SIZE)
 *   - Handle validation via loot_item_defs API
 *   - Basic statistics (hits/misses/evictions)
 *   - Memory usage tracking (approximate: sum of frame bit + advanced buffers)
 * Deferred (future slices): background loading, memory pressure adaptive eviction, timestamp
 * tracking via asset mtime, RW locks, prefetch heuristics.
 */
#pragma once
#include "core/loot/loot_item_defs.h"
#include "game/hit_pixel_mask.h"
#include "game/pixel_mask_loader.h"
#include <stddef.h>
#include <stdint.h>

/* Alias a generic pixel mask set to the weapon-centric RogueHitPixelMaskSet for the initial slice.
    Future slices may introduce a distinct structure (animation-set agnostic). */
typedef RogueHitPixelMaskSet RoguePixelMaskSet;

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_COLLISION_CACHE_SIZE
#define ROGUE_COLLISION_CACHE_SIZE 128
#endif
#ifndef ROGUE_COLLISION_CACHE_MAX_MEMORY_MB
#define ROGUE_COLLISION_CACHE_MAX_MEMORY_MB 64
#endif

    typedef struct RogueItemCollisionCacheEntry
    {
        RogueItemDefHandle handle;
        RoguePixelMaskSet* mask_set; /* lazily allocated/filled */
        uint64_t last_access_tick;   /* monotonic counter (not time-based) */
        uint64_t asset_timestamp;    /* file mtime snapshot when built (0 if unknown) */
        uint32_t access_count;
        uint32_t generation_snapshot; /* to detect stale handles */
        struct RogueItemCollisionCacheEntry* prev;
        struct RogueItemCollisionCacheEntry* next;
    } RogueItemCollisionCacheEntry;

    typedef struct RogueItemCollisionCacheStats
    {
        uint64_t lookups;
        uint64_t hits;
        uint64_t misses;
        uint64_t evictions;
        uint64_t invalidations; /* explicit or timestamp-based */
        size_t approx_bytes;    /* current resident memory */
    } RogueItemCollisionCacheStats;

    /* Initialize cache (idempotent). */
    void rogue_item_collision_cache_init(void);
    /* Reset cache (frees all stored mask sets). */
    void rogue_item_collision_cache_reset(void);
    /* Retrieve (and lazily build) collision mask set for an item handle. Returns NULL on failure.
     */
    RoguePixelMaskSet* rogue_item_collision_cache_get(RogueItemDefHandle handle,
                                                      const RoguePixelMaskLoadConfig* cfg);
    /* Query current stats. */
    RogueItemCollisionCacheStats rogue_item_collision_cache_get_stats(void);
    /* Manually invalidate a specific handle (all generations equal). Safe no-op if not cached. */
    void rogue_item_collision_cache_invalidate_handle(RogueItemDefHandle handle);
    /* Invalidate every cached entry (lighter than full reset: keeps stats except bytes). */
    void rogue_item_collision_cache_invalidate_all(void);

    /* -------- Dynamic limits (deterministic enforcement) -------- */
    /* Set effective entry capacity (<= compile-time array size) and memory limit in MB.
        Values are clamped to valid ranges. Enforcement occurs immediately (LRU evictions)
        and is deterministic (evict from LRU tail). Passing a non-positive max_entries sets
        the effective capacity to 0 (cache holds no entries). */
    void rogue_item_collision_cache_set_limits(int max_entries, size_t max_memory_mb);
    /* Read back current effective limits; any out parameter may be NULL. */
    void rogue_item_collision_cache_get_limits(int* out_max_entries, size_t* out_max_memory_mb);

    /* -------- Background loading & hot-reload (Phase 1.2 extension) -------- */
    struct RogueThreadPool; /* fwd */
    /* Register a thread pool to enable background requests. Pass NULL to disable. */
    void rogue_item_collision_cache_set_thread_pool(struct RogueThreadPool* tp);
    /* Enqueue an asynchronous build for an item handle if not cached.
        Returns 1 if the item is already cached or the request was queued/synchronously built, 0 on
       error. */
    int rogue_item_collision_cache_request_async(RogueItemDefHandle handle,
                                                 const RoguePixelMaskLoadConfig* cfg);
    /* Returns 1 if the cache currently contains a ready mask set for the handle. */
    int rogue_item_collision_cache_is_ready(RogueItemDefHandle handle);
    /* Poll a limited number of cache entries and invalidate those whose source asset timestamps
        have advanced (hot-reload friendly). Returns number of invalidated entries this call. */
    int rogue_item_collision_cache_poll(int max_to_check);
    /* Testing/override: set a hook to compute file modification time (in seconds since epoch).
        When set (non-NULL), this hook is used instead of the internal mtime probe. */
    void rogue_item_collision_cache_set_mtime_hook(uint64_t (*hook)(const char* path));

    /* -------- Prefetch heuristics & finer-grained invalidation (Phase 1.2+) -------- */
    /* Enable/disable usage-based prefetch and set a deterministic budget (number of related
        items, scanned by ascending item index). When enabled and a cache build completes,
        related items that share the same sprite sheet path may be queued for background build
        if a thread pool is registered. Defaults: enabled=1, budget=2. */
    void rogue_item_collision_cache_set_prefetch(int enabled, int budget);

    /* Invalidate all cache entries that reference the given sprite sheet path. The path may be
        provided as a bare filename (assumed under assets/) or a full/relative path; matching is
        performed after normalizing to the same assets/… form used by the cache. */
    void rogue_item_collision_cache_invalidate_sprite(const char* sprite_path);

    /* Optimize the cache in-place: compact the LRU list by removing tombstones, rebuild the
        MRU->LRU ordering deterministically based on last_access_tick, and recompute memory stats.
        This does not free valid entries or change capacity; it is safe to call at any time and is
        intended for maintenance during loading screens or at low-frequency service points. */
    void rogue_item_collision_cache_optimize(void);

    /* -------- Analytics snapshot (deterministic, MRU->LRU order) -------- */
    typedef struct RogueItemCollisionCacheEntryInfo
    {
        RogueItemDefHandle handle;
        uint32_t access_count;
        uint64_t last_access_tick;
        uint64_t asset_timestamp;
        size_t approx_bytes; /* per-entry approximate resident memory */
    } RogueItemCollisionCacheEntryInfo;

    /*
        Capture a stable snapshot of the current cache contents in MRU->LRU order.
        The function fills up to max_entries elements of the provided out array and
        returns the number of entries written. The snapshot is taken under the
        cache read lock and is deterministic. Passing NULL for out returns the
        number of entries without writing data (count query).
    */
    size_t rogue_item_collision_cache_snapshot(RogueItemCollisionCacheEntryInfo* out,
                                               size_t max_entries);

    /* -------- Usage-driven dynamic sizing advisory (read-only) -------- */
    typedef struct RogueItemCollisionCacheAdvisory
    {
        /* Derived recommendations (read-only; does not mutate cache limits) */
        int recommended_max_entries;      /* based on recent MRU window with headroom */
        size_t recommended_max_memory_mb; /* based on approx_bytes percentiles + entries */
        /* Diagnostics */
        int alive_entries; /* total alive entries at snapshot */
        int recent_window; /* size of MRU window considered (<= alive, <= 32) */
        size_t p50_bytes;  /* median approx_bytes across alive entries */
        size_t p90_bytes;  /* 90th percentile approx_bytes across alive entries */
        size_t p99_bytes;  /* 99th percentile approx_bytes across alive entries */
    } RogueItemCollisionCacheAdvisory;

    /*
            Compute a usage-driven advisory for cache sizing without mutating state.
            The function analyzes the current contents in a deterministic snapshot:
                - recent_window = min(alive, 32) most-recently-used entries
                - recommended_max_entries = recent_window + recent_window/2 (1.5x headroom),
                    clamped to [0, ROGUE_COLLISION_CACHE_SIZE]
                - p50/p90/p99 are computed across alive entries' approx_bytes
                - recommended_max_memory_mb = ceil(p90_bytes * recommended_max_entries / 1MiB),
                    with a floor of 1 MiB when any entries exist (0 when none)
            All values are deterministic given a fixed cache state. Returns results via 'out'.
    */
    void rogue_item_collision_cache_get_advisory(RogueItemCollisionCacheAdvisory* out);

    /* Helper: restore compile-time default limits deterministically. */
    void rogue_item_collision_cache_set_limits_default(void);

#ifdef __cplusplus
}
#endif
