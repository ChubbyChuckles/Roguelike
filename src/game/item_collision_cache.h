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

#ifdef __cplusplus
}
#endif
