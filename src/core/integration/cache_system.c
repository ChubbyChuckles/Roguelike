/**
 * @file cache_system.c
 * @brief Multi-level caching system with compression and statistics.
 *
 * This module implements a three-level caching system (L1, L2, L3) designed for
 * high-performance data storage and retrieval. The cache uses open addressing
 * hash tables with linear probing and supports automatic data compression using
 * Run-Length Encoding (RLE) for space efficiency.
 *
 * Key features:
 * - Three cache levels with different size thresholds (L1 ≤256B, L2 ≤4KB, L3 >4KB)
 * - Automatic promotion from lower to higher levels on cache hits
 * - RLE compression for entries above configurable threshold
 * - Comprehensive statistics tracking (hits, misses, evictions, etc.)
 * - Tombstone-based deletion for efficient space reuse
 * - Preloading support for bulk cache population
 *
 * The cache is optimized for scenarios with frequent access to small-to-medium
 * sized data objects and provides detailed performance metrics for monitoring
 * cache effectiveness.
 */

#include "cache_system.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Simple hash map per level with open addressing linear probe.
// Compression: naive RLE (byte,run) pairs if saves >= (raw_size/8) and size >= threshold.

// Simple hash map per level with open addressing linear probe.
// Compression: naive RLE (byte,run) pairs if saves >= (raw_size/8) and size >= threshold.

/**
 * @brief Default capacity for L1 cache level.
 *
 * L1 cache is optimized for small data objects (≤256 bytes).
 * This capacity represents the number of entries, not bytes.
 */
#define ROGUE_CACHE_DEFAULT_L1 256

/**
 * @brief Default capacity for L2 cache level.
 *
 * L2 cache handles medium-sized data objects (256-4096 bytes).
 * This capacity represents the number of entries, not bytes.
 */
#define ROGUE_CACHE_DEFAULT_L2 512

/**
 * @brief Default capacity for L3 cache level.
 *
 * L3 cache stores larger data objects (>4096 bytes).
 * This capacity represents the number of entries, not bytes.
 */
#define ROGUE_CACHE_DEFAULT_L3 1024

/**
 * @brief Internal cache entry structure.
 *
 * Represents a single cached item with metadata for storage,
 * compression, and cache management.
 */
typedef struct CacheEntry
{
    uint64_t key;            /**< Unique identifier for the cached item */
    uint32_t version;        /**< Version number for cache invalidation */
    uint32_t level;          /**< Current cache level (0=L1, 1=L2, 2=L3) */
    uint32_t raw_size;       /**< Original uncompressed data size in bytes */
    uint32_t data_size;      /**< Stored data size (compressed or raw) in bytes */
    unsigned compressed : 1; /**< Flag indicating if data is compressed */
    unsigned tombstone : 1;  /**< Flag for logical deletion (tombstone) */
    unsigned pad : 30;       /**< Padding to align structure */
    void* data;              /**< Owned buffer containing the cached data */
} CacheEntry;

/**
 * @brief Internal cache level structure.
 *
 * Manages a single cache level with its hash table and statistics.
 */
typedef struct CacheLevel
{
    CacheEntry* entries; /**< Array of cache entries (hash table) */
    size_t capacity;     /**< Total number of slots in the hash table */
    size_t count;        /**< Number of live entries (excluding tombstones) */
} CacheLevel;

/** @brief Global cache level storage - one per cache level (L1, L2, L3) */
static CacheLevel s_levels[ROGUE_CACHE_LEVELS];

/** @brief Maximum entries per cache level before eviction is triggered */
static size_t s_capacity_entries[ROGUE_CACHE_LEVELS];

/** @brief Hit counters per cache level for performance statistics */
static uint64_t s_hits[ROGUE_CACHE_LEVELS];

/** @brief Miss counters per cache level for performance statistics */
static uint64_t s_misses[ROGUE_CACHE_LEVELS];

/** @brief Eviction counters per cache level for performance statistics */
static uint64_t s_evictions[ROGUE_CACHE_LEVELS];

/** @brief Invalidation counters per cache level for performance statistics */
static uint64_t s_invalidations[ROGUE_CACHE_LEVELS];

/** @brief Promotion counters per cache level for performance statistics */
static uint64_t s_promotions[ROGUE_CACHE_LEVELS];

/** @brief Total number of compressed cache entries across all levels */
static uint64_t s_compressed_entries = 0;

/** @brief Total bytes saved through compression across all entries */
static size_t s_compressed_saved = 0;

/** @brief Total number of preload operations performed */
static uint64_t s_preloads = 0;

/** @brief Minimum data size threshold for compression (default: 1024 bytes) */
/** @brief Minimum data size threshold for compression (default: 1024 bytes) */
static size_t s_compress_threshold = 1024;

/**
 * @brief Hash function for cache keys using xorshift mixing.
 *
 * Generates a high-quality 64-bit hash from a 64-bit key using
 * xorshift multiplication. This provides good distribution for
 * hash table indexing.
 *
 * @param k The 64-bit key to hash
 * @return A 64-bit hash value suitable for hash table indexing
 */
static uint64_t hash_key(uint64_t k)
{ // xorshift mix
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

/**
 * @brief Calculates the next power of 2 greater than or equal to the input.
 *
 * Used for sizing hash tables to ensure efficient bit masking operations.
 * Handles edge cases for small inputs and supports both 32-bit and 64-bit systems.
 *
 * @param x The input value
 * @return The smallest power of 2 that is >= x, or 1 if x <= 1
 */
static size_t next_pow2(size_t x)
{
    if (x <= 1)
        return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
#if SIZE_MAX > 0xFFFFFFFFu
    x |= x >> 32;
#endif
    return x + 1;
}

/**
 * @brief Initializes a cache level with the specified capacity.
 *
 * Allocates memory for the cache level's hash table and initializes
 * the structure. The actual table size will be twice the capacity
 * to maintain a load factor of approximately 0.5.
 *
 * @param lvl Pointer to the CacheLevel structure to initialize
 * @param cap The logical capacity (number of entries) for this level
 * @return 0 on success, -1 on memory allocation failure
 */
static int level_init(CacheLevel* lvl, size_t cap)
{
    lvl->capacity = cap;
    lvl->entries = (CacheEntry*) calloc(cap, sizeof(CacheEntry));
    return lvl->entries ? 0 : -1;
}

/**
 * @brief Initializes the multi-level cache system.
 *
 * Sets up the three cache levels (L1, L2, L3) with the specified capacities.
 * If any capacity is 0, default values are used. The actual hash table sizes
 * are doubled to maintain optimal load factors.
 *
 * @param cap_l1 Maximum number of entries for L1 cache (default: 256)
 * @param cap_l2 Maximum number of entries for L2 cache (default: 512)
 * @param cap_l3 Maximum number of entries for L3 cache (default: 1024)
 * @return 0 on success, -1 on memory allocation failure
 *
 * @note This function must be called before using any other cache operations.
 *       Call rogue_cache_shutdown() to clean up resources when done.
 */
int rogue_cache_init(size_t cap_l1, size_t cap_l2, size_t cap_l3)
{
    if (cap_l1 == 0)
        cap_l1 = ROGUE_CACHE_DEFAULT_L1;
    if (cap_l2 == 0)
        cap_l2 = ROGUE_CACHE_DEFAULT_L2;
    if (cap_l3 == 0)
        cap_l3 = ROGUE_CACHE_DEFAULT_L3;
    cap_l1 = next_pow2(cap_l1 * 2);
    cap_l2 = next_pow2(cap_l2 * 2);
    cap_l3 = next_pow2(cap_l3 * 2); // load factor target 0.5
    if (level_init(&s_levels[0], cap_l1) || level_init(&s_levels[1], cap_l2) ||
        level_init(&s_levels[2], cap_l3))
        return -1;
    s_capacity_entries[0] = cap_l1 / 2;
    s_capacity_entries[1] = cap_l2 / 2;
    s_capacity_entries[2] =
        cap_l3 /
        2; // logical capacity (#entries before rehash needed; we don't resize in this slice)
    return 0;
}

/**
 * @brief Shuts down the cache system and frees all resources.
 *
 * Cleans up all cache levels by freeing allocated data buffers and
 * hash table memory. Also resets all statistics counters to zero.
 * After calling this function, the cache is in an uninitialized state
 * and must be re-initialized before use.
 *
 * @note This function is safe to call even if the cache was never initialized
 *       or has already been shut down.
 */
void rogue_cache_shutdown(void)
{
    for (int i = 0; i < ROGUE_CACHE_LEVELS; i++)
    {
        CacheLevel* lvl = &s_levels[i];
        if (lvl->entries)
        {
            for (size_t j = 0; j < lvl->capacity; j++)
            {
                CacheEntry* e = &lvl->entries[j];
                if (e->key && !e->tombstone && e->data)
                    free(e->data);
            }
            free(lvl->entries);
            memset(lvl, 0, sizeof(*lvl));
        }
    }
    memset(s_hits, 0, sizeof(s_hits));
    memset(s_misses, 0, sizeof(s_misses));
    memset(s_evictions, 0, sizeof(s_evictions));
    memset(s_invalidations, 0, sizeof(s_invalidations));
    memset(s_promotions, 0, sizeof(s_promotions));
    s_compressed_entries = 0;
    s_compressed_saved = 0;
    s_preloads = 0;
}

/**
 * @brief Compresses data using Run-Length Encoding (RLE).
 *
 * Applies simple RLE compression where sequences of identical bytes
 * are encoded as (byte, count) pairs. Only compresses if the result
 * saves at least 1/8 of the original size.
 *
 * @param data Pointer to the data to compress
 * @param size Size of the input data in bytes
 * @param out_size Pointer to store the size of compressed data
 * @return Pointer to compressed data buffer (caller must free), or NULL if compression
 *         wouldn't save space or allocation failed
 *
 * @note The compression threshold is checked before calling this function,
 *       so this is primarily an internal utility.
 */
static void* rle_compress(const void* data, size_t size, size_t* out_size)
{
    const unsigned char* in = (const unsigned char*) data;
    unsigned char* out = (unsigned char*) malloc(size * 2);
    if (!out)
        return NULL;
    size_t w = 0;
    for (size_t i = 0; i < size;)
    {
        unsigned char b = in[i];
        size_t run = 1;
        while (i + run < size && in[i + run] == b && run < 255)
            run++;
        out[w++] = b;
        out[w++] = (unsigned char) run;
        i += run;
    }
    if (w >= size - size / 8)
    {
        free(out);
        return NULL;
    }
    *out_size = w;
    return out;
}

/**
 * @brief Finds a cache entry slot for the given key using linear probing.
 *
 * Searches the hash table for an existing entry with the specified key.
 * Uses linear probing to handle hash collisions. If no existing entry
 * is found, returns the best available slot for insertion.
 *
 * @param lvl Pointer to the cache level to search
 * @param key The key to search for
 * @param out_index Optional pointer to store the slot index found
 * @return Pointer to existing CacheEntry if found and not tombstoned,
 *         NULL if key not found (out_index contains insertion slot)
 */
static CacheEntry* find_slot(CacheLevel* lvl, uint64_t key, size_t* out_index)
{
    size_t mask = lvl->capacity - 1;
    uint64_t h = hash_key(key);
    size_t idx = (size_t) h & mask;
    size_t first_tomb = (size_t) -1;
    for (size_t probe = 0; probe < lvl->capacity; probe++)
    {
        CacheEntry* e = &lvl->entries[idx];
        if (!e->key)
        {
            if (out_index)
            {
                *out_index = (first_tomb != (size_t) -1) ? first_tomb : idx;
            }
            return NULL;
        }
        if (e->key == key && !e->tombstone)
        {
            if (out_index)
                *out_index = idx;
            return e;
        }
        if (e->tombstone && first_tomb == (size_t) -1)
            first_tomb = idx;
        idx = (idx + 1) & mask;
    }
    return NULL;
}

/**
 * @brief Inserts or updates an entry in the specified cache level.
 *
 * Handles both insertion of new entries and updates of existing ones.
 * If the cache level is at capacity, performs eviction of the oldest entry.
 * Automatically applies compression if the data size meets the threshold.
 *
 * @param level The cache level (0=L1, 1=L2, 2=L3) to insert into
 * @param key Unique identifier for the cache entry
 * @param data Pointer to the data to cache
 * @param size Size of the data in bytes
 * @param version Version number for cache invalidation
 * @return 0 on success, -1 on memory allocation failure
 */
static int insert_entry(int level, uint64_t key, const void* data, size_t size, uint32_t version)
{
    CacheLevel* lvl = &s_levels[level];
    if (lvl->count >= s_capacity_entries[level])
    { // simple eviction: linear scan remove oldest tombstone or first live
        for (size_t i = 0; i < lvl->capacity; i++)
        {
            CacheEntry* e = &lvl->entries[i];
            if (e->key && !e->tombstone)
            {
                if (e->data)
                    free(e->data);
                e->tombstone = 1;
                s_evictions[level]++;
                lvl->count--;
                break;
            }
        }
    }
    size_t idx;
    CacheEntry* existing = find_slot(lvl, key, &idx);
    if (existing)
    { // update
        if (existing->data)
            free(existing->data);
        existing->version = version;
        existing->level = level;
        existing->compressed = 0;
        existing->raw_size = (uint32_t) size;
        existing->tombstone = 0;
        size_t csize = 0;
        void* cbuf = (s_compress_threshold && size >= s_compress_threshold)
                         ? rle_compress(data, size, &csize)
                         : NULL;
        if (cbuf)
        {
            existing->data = cbuf;
            existing->data_size = (uint32_t) csize;
            existing->compressed = 1;
            s_compressed_entries++;
            s_compressed_saved += size - csize;
        }
        else
        {
            existing->data = malloc(size);
            if (!existing->data)
                return -1;
            memcpy(existing->data, data, size);
            existing->data_size = (uint32_t) size;
        }
        return 0;
    }
    CacheEntry* e = &lvl->entries[idx];
    if (!e->key || e->tombstone)
    {
        if (!e->key)
            lvl->count++;
        e->key = key;
        e->tombstone = 0;
        e->version = version;
        e->level = level;
        e->compressed = 0;
        e->raw_size = (uint32_t) size;
        size_t csize = 0;
        void* cbuf = (s_compress_threshold && size >= s_compress_threshold)
                         ? rle_compress(data, size, &csize)
                         : NULL;
        if (cbuf)
        {
            e->data = cbuf;
            e->data_size = (uint32_t) csize;
            e->compressed = 1;
            s_compressed_entries++;
            s_compressed_saved += size - csize;
        }
        else
        {
            e->data = malloc(size);
            if (!e->data)
                return -1;
            memcpy(e->data, data, size);
            e->data_size = (uint32_t) size;
        }
        return 0;
    }
    return -1;
}

/**
 * @brief Determines the appropriate cache level for data of the given size.
 *
 * Uses size-based heuristics to select the optimal cache level:
 * - L1 (level 0): Small data ≤ 256 bytes
 * - L2 (level 1): Medium data 257-4096 bytes
 * - L3 (level 2): Large data > 4096 bytes
 *
 * @param size Size of the data in bytes
 * @return The recommended cache level (0, 1, or 2)
 */
static int pick_level(size_t size)
{
    if (size <= 256)
        return 0;
    if (size <= 4096)
        return 1;
    return 2;
}

/**
 * @brief Stores data in the cache with automatic level selection.
 *
 * Inserts or updates a cache entry with the specified key and data.
 * If level_hint is invalid (< 0 or >= ROGUE_CACHE_LEVELS), automatically
 * selects the appropriate cache level based on data size.
 *
 * @param key Unique identifier for the cache entry
 * @param data Pointer to the data to store
 * @param size Size of the data in bytes
 * @param version Version number for cache invalidation
 * @param level_hint Preferred cache level, or -1 for automatic selection
 * @return 0 on success, -1 on memory allocation failure
 *
 * @note Data is automatically compressed if size >= compression threshold
 *       and compression would save space.
 */
int rogue_cache_put(uint64_t key, const void* data, size_t size, uint32_t version, int level_hint)
{
    if (level_hint < 0 || level_hint >= ROGUE_CACHE_LEVELS)
        level_hint = pick_level(size);
    return insert_entry(level_hint, key, data, size, version);
}

/**
 * @brief Retrieves data from the cache with automatic promotion.
 *
 * Searches all cache levels for the specified key, starting from L1.
 * If found in L2 or L3, automatically promotes the entry to L1 for
 * faster future access.
 *
 * @param key Unique identifier for the cache entry
 * @param out_data Pointer to store the retrieved data pointer (owned by cache)
 * @param out_size Pointer to store the data size in bytes
 * @param out_version Pointer to store the entry version number
 * @return 1 if found, 0 if not found
 *
 * @note The returned data pointer is owned by the cache and should not be freed.
 *       It remains valid until the entry is invalidated or the cache is shut down.
 */
int rogue_cache_get(uint64_t key, void** out_data, size_t* out_size, uint32_t* out_version)
{
    for (int lvl = 0; lvl < ROGUE_CACHE_LEVELS; lvl++)
    {
        CacheLevel* L = &s_levels[lvl];
        size_t idx;
        CacheEntry* e = find_slot(L, key, &idx);
        if (e)
        {
            s_hits[lvl]++;
            if (out_data)
                *out_data = e->data;
            if (out_size)
                *out_size = e->raw_size;
            if (out_version)
                *out_version = e->version; // promote if not L1
            if (lvl > 0)
            { // reinsert at higher priority (L1)
                insert_entry(0, key, e->data, e->raw_size, e->version);
                s_promotions[0]++;
                s_promotions[lvl]++;
            }
            return 1;
        }
    }
    for (int lvl = 0; lvl < ROGUE_CACHE_LEVELS; lvl++)
        s_misses[lvl]++;
    return 0;
}

/**
 * @brief Invalidates a specific cache entry across all levels.
 *
 * Marks the entry with the specified key as invalid (tombstoned) in all
 * cache levels. The entry's memory is freed and the slot becomes available
 * for reuse. This is useful for cache invalidation when underlying data changes.
 *
 * @param key Unique identifier of the cache entry to invalidate
 *
 * @note This operation affects all cache levels and is safe to call
 *       even if the key doesn't exist in the cache.
 */
void rogue_cache_invalidate(uint64_t key)
{
    for (int lvl = 0; lvl < ROGUE_CACHE_LEVELS; lvl++)
    {
        CacheLevel* L = &s_levels[lvl];
        size_t mask = L->capacity - 1;
        uint64_t h = hash_key(key);
        size_t idx = (size_t) h & mask;
        for (size_t probe = 0; probe < L->capacity; probe++)
        {
            CacheEntry* e = &L->entries[idx];
            if (!e->key)
                break;
            if (e->key == key && !e->tombstone)
            {
                if (e->data)
                    free(e->data);
                e->data = NULL;
                e->tombstone = 1;
                L->count--;
                s_invalidations[lvl]++;
                break;
            }
            idx = (idx + 1) & mask;
        }
    }
}

/**
 * @brief Invalidates all cache entries across all levels.
 *
 * Clears the entire cache by marking all entries as invalid (tombstoned).
 * This is useful for bulk cache invalidation, such as when switching
 * game sessions or when external data sources change significantly.
 *
 * @note This operation preserves the cache structure but removes all data.
 *       Statistics counters for invalidations are updated accordingly.
 */
void rogue_cache_invalidate_all(void)
{
    for (int lvl = 0; lvl < ROGUE_CACHE_LEVELS; lvl++)
    {
        CacheLevel* L = &s_levels[lvl];
        for (size_t i = 0; i < L->capacity; i++)
        {
            CacheEntry* e = &L->entries[i];
            if (e->key && !e->tombstone)
            {
                if (e->data)
                    free(e->data);
                e->data = NULL;
                e->tombstone = 1;
            }
        }
        s_invalidations[lvl] += L->count;
        L->count = 0;
    }
}

/**
 * @brief Preloads multiple cache entries using a loader function.
 *
 * Bulk loads cache entries for the specified keys using a user-provided
 * loader function. This is useful for warming up the cache with frequently
 * accessed data. Failed loads are silently ignored.
 *
 * @param keys Array of unique identifiers to preload
 * @param count Number of keys in the array
 * @param target_level Cache level to store preloaded entries (default: L2)
 * @param loader Function pointer that loads data for a given key
 * @return Number of successfully preloaded entries
 *
 * @note The loader function signature: int loader(uint64_t key, void** data, size_t* size,
 * uint32_t* version) Should return 0 on success, non-zero on failure. The returned data buffer is
 * owned by the caller.
 */
int rogue_cache_preload(const uint64_t* keys, int count, RogueCacheLevel target_level,
                        int (*loader)(uint64_t, void**, size_t*, uint32_t*))
{
    if (target_level < 0 || target_level >= ROGUE_CACHE_LEVELS)
        target_level = ROGUE_CACHE_L2;
    int loaded = 0;
    for (int i = 0; i < count; i++)
    {
        void* buf = NULL;
        size_t sz = 0;
        uint32_t ver = 0;
        if (loader(keys[i], &buf, &sz, &ver) == 0)
        {
            if (insert_entry(target_level, keys[i], buf, sz, ver) == 0)
            {
                loaded++;
                s_preloads++;
            }
            if (buf)
                free(buf);
        }
    }
    return loaded;
}

/**
 * @brief Retrieves comprehensive cache statistics.
 *
 * Populates the provided stats structure with current cache performance
 * metrics including hits, misses, evictions, and compression statistics
 * for all cache levels.
 *
 * @param out Pointer to RogueCacheStats structure to populate
 *
 * @note Safe to call with NULL pointer (no-op in that case).
 *       All statistics are reset to zero when the cache is shut down.
 */
void rogue_cache_get_stats(RogueCacheStats* out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < ROGUE_CACHE_LEVELS; i++)
    {
        out->level_capacity[i] = s_capacity_entries[i];
        out->level_entries[i] = s_levels[i].count;
        out->level_hits[i] = s_hits[i];
        out->level_misses[i] = s_misses[i];
        out->level_evictions[i] = s_evictions[i];
        out->level_invalidations[i] = s_invalidations[i];
        out->level_promotions[i] = s_promotions[i];
    }
    out->compressed_entries = s_compressed_entries;
    out->compressed_bytes_saved = s_compressed_saved;
    out->preload_operations = s_preloads;
}

/**
 * @brief Prints cache statistics to stdout.
 *
 * Outputs a formatted summary of cache performance including per-level
 * statistics and compression metrics. Useful for debugging and monitoring.
 *
 * Output format:
 * [cache]
 * L0: entries=X cap=Y hits=Z misses=W evict=V inval=U promo=P
 * L1: ...
 * L2: ...
 * compressed=C saved=S preload=P
 */
void rogue_cache_dump(void)
{
    RogueCacheStats s;
    rogue_cache_get_stats(&s);
    printf("[cache]\n");
    for (int i = 0; i < ROGUE_CACHE_LEVELS; i++)
    {
        printf(" L%d: entries=%zu cap=%zu hits=%llu misses=%llu evict=%llu inval=%llu promo=%llu\n",
               i, s.level_entries[i], s.level_capacity[i], (unsigned long long) s.level_hits[i],
               (unsigned long long) s.level_misses[i], (unsigned long long) s.level_evictions[i],
               (unsigned long long) s.level_invalidations[i],
               (unsigned long long) s.level_promotions[i]);
    }
    printf(" compressed=%llu saved=%zu preload=%llu\n", (unsigned long long) s.compressed_entries,
           s.compressed_bytes_saved, (unsigned long long) s.preload_operations);
}

/**
 * @brief Iterates over all cache entries with a user callback.
 *
 * Calls the provided function for each live cache entry across all levels.
 * Iteration continues until all entries are processed or the callback
 * returns false (non-zero).
 *
 * @param fn Callback function to invoke for each entry
 * @param ud User data pointer passed to the callback
 *
 * @note Callback signature: int fn(uint64_t key, void* data, size_t size, uint32_t version, int
 * level, void* ud) Return true (non-zero) to continue iteration, false to stop.
 */
void rogue_cache_iterate(RogueCacheIterFn fn, void* ud)
{
    if (!fn)
        return;
    for (int lvl = 0; lvl < ROGUE_CACHE_LEVELS; lvl++)
    {
        CacheLevel* L = &s_levels[lvl];
        for (size_t i = 0; i < L->capacity; i++)
        {
            CacheEntry* e = &L->entries[i];
            if (e->key && !e->tombstone)
            {
                if (!fn(e->key, e->data, e->raw_size, e->version, lvl, ud))
                    return;
            }
        }
    }
}

/**
 * @brief Manually promotes a cache entry to a higher level.
 *
 * Searches for the specified key starting from the highest level (L3)
 * and promotes it to the next higher level if found. This is useful
 * for manually adjusting cache priorities.
 *
 * @param key Unique identifier of the cache entry to promote
 *
 * @note Only promotes one level at a time. Multiple calls may be needed
 *       to reach L1 from L3. Statistics counters are updated accordingly.
 */
void rogue_cache_promote(uint64_t key)
{
    for (int lvl = ROGUE_CACHE_LEVELS - 1; lvl > 0; lvl--)
    {
        CacheLevel* L = &s_levels[lvl];
        size_t idx;
        CacheEntry* e = find_slot(L, key, &idx);
        if (e)
        {
            insert_entry(lvl - 1, key, e->data, e->raw_size, e->version);
            s_promotions[lvl - 1]++;
            s_promotions[lvl]++;
            break;
        }
    }
}

/**
 * @brief Sets the minimum size threshold for data compression.
 *
 * Configures the minimum data size required for compression to be attempted.
 * Data smaller than this threshold is stored uncompressed. Setting to 0
 * disables compression entirely.
 *
 * @param bytes Minimum data size in bytes for compression (default: 1024)
 *
 * @note This setting affects future insertions but not existing compressed entries.
 *       Smaller thresholds increase CPU usage but may improve space efficiency.
 */
void rogue_cache_set_compress_threshold(size_t bytes) { s_compress_threshold = bytes; }
