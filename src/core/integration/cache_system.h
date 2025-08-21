// Cache Management & Invalidation System (Phase 4.3)
// Multi-level cache (L1 hot, L2 warm, L3 cold) with coherence, invalidation,
// preloading, compression (simple RLE), statistics & debugging utilities.
// C implementation (no C++); thread safety not yet required (future phases).

#ifndef ROGUE_CACHE_SYSTEM_H
#define ROGUE_CACHE_SYSTEM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROGUE_CACHE_LEVELS 3

typedef enum RogueCacheLevel { ROGUE_CACHE_L1=0, ROGUE_CACHE_L2=1, ROGUE_CACHE_L3=2 } RogueCacheLevel;

// Stats snapshot per-level + aggregated
typedef struct RogueCacheStats {
    size_t level_capacity[ROGUE_CACHE_LEVELS];
    size_t level_entries[ROGUE_CACHE_LEVELS];
    uint64_t level_hits[ROGUE_CACHE_LEVELS];
    uint64_t level_misses[ROGUE_CACHE_LEVELS];
    uint64_t level_evictions[ROGUE_CACHE_LEVELS];
    uint64_t level_invalidations[ROGUE_CACHE_LEVELS];
    uint64_t level_promotions[ROGUE_CACHE_LEVELS]; // number of entries promoted into this level
    uint64_t compressed_entries; // total entries stored compressed
    size_t   compressed_bytes_saved; // total raw_size - compressed_size accumulated
    uint64_t preload_operations; // successful preloads
} RogueCacheStats;

// Initialize cache with per-level capacity (entry counts). 0 => default sizes.
int  rogue_cache_init(size_t cap_l1, size_t cap_l2, size_t cap_l3);
void rogue_cache_shutdown(void);

// Insert/update entry. If level_hint outside range choose appropriate level (L1 for small <=256 bytes, else L2 else L3).
// Returns 0 success, -1 on allocation/other failure.
int rogue_cache_put(uint64_t key, const void *data, size_t size, uint32_t version, int level_hint);

// Get entry; returns 1 if found (and outputs), 0 if not.
// On success out pointers remain valid until invalidated or freed at shutdown.
int rogue_cache_get(uint64_t key, void **out_data, size_t *out_size, uint32_t *out_version);

// Invalidate specific key (all levels) or all.
void rogue_cache_invalidate(uint64_t key);
void rogue_cache_invalidate_all(void);

// Preload keys into target level using provided loader callback.
// loader(key, &data_ptr, &size, &version) should allocate data buffer (caller frees on failure) –
// cache takes ownership on success.
int rogue_cache_preload(const uint64_t *keys, int count, RogueCacheLevel target_level,
                        int (*loader)(uint64_t, void **, size_t *, uint32_t *));

// Stats & diagnostics
void rogue_cache_get_stats(RogueCacheStats *out);
void rogue_cache_dump(void); // human-readable summary to stdout

// Iterate across all live entries (for debugging/export). The callback may return false to stop.
typedef bool (*RogueCacheIterFn)(uint64_t key, const void *data, size_t size, uint32_t version, int level, void *ud);
void rogue_cache_iterate(RogueCacheIterFn fn, void *ud);

// Force promotion of a key (if present) to the next hotter level.
void rogue_cache_promote(uint64_t key);

// Compression threshold control (default 1024). Setting 0 disables compression.
void rogue_cache_set_compress_threshold(size_t bytes);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_CACHE_SYSTEM_H
