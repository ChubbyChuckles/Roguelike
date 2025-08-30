/**
 * @file world_gen_stream.c
 * @brief Implements runtime streaming and caching for world chunks.
 *
 * This module provides a chunk streaming manager for generating and caching world chunks
 * on-demand, with LRU eviction and optional persistent caching. Phase 11: Runtime Streaming &
 * Caching Implementation
 */

#include "tilemap.h"
#include "world_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ROGUE_STREAM_MAX_QUEUE
/**
 * @brief Maximum number of chunks that can be queued for generation.
 */
#define ROGUE_STREAM_MAX_QUEUE 512
#endif

/**
 * @brief Represents a generated world chunk.
 */
struct RogueGeneratedChunk
{
    int cx;                         /**< Chunk X coordinate. */
    int cy;                         /**< Chunk Y coordinate. */
    RogueTileMap map;               /**< Chunk-sized tile map. */
    unsigned long long hash;        /**< Hash of the chunk's tiles. */
    unsigned long last_access_tick; /**< Last access tick for LRU. */
};

/**
 * @brief Queue item for chunk generation requests.
 */
typedef struct RogueChunkQueueItem
{
    int cx; /**< Chunk X coordinate. */
    int cy; /**< Chunk Y coordinate. */
} RogueChunkQueueItem;

/**
 * @brief Cache entry for a chunk.
 */
typedef struct RogueChunkCacheEntry
{
    struct RogueGeneratedChunk* chunk; /**< Pointer to the chunk. */
    int in_use;                        /**< Whether the entry is in use. */
} RogueChunkCacheEntry;

/**
 * @brief Manager for chunk streaming and caching.
 */
struct RogueChunkStreamManager
{
    RogueWorldGenConfig base_cfg;                      /**< Copied base configuration. */
    RogueChunkQueueItem queue[ROGUE_STREAM_MAX_QUEUE]; /**< Generation queue. */
    int q_head;                                        /**< Queue head index. */
    int q_tail;                                        /**< Queue tail index. */
    int q_count;                                       /**< Number of items in queue. */
    RogueChunkCacheEntry* entries;                     /**< Cache entries array. */
    int capacity;                                      /**< Cache capacity. */
    int loaded;                                        /**< Number of loaded chunks. */
    RogueChunkStreamStats stats;                       /**< Streaming statistics. */
    int budget_per_tick;                               /**< Processing budget per tick. */
    unsigned long global_tick;                         /**< Global tick counter. */
    char cache_dir[260];                               /**< Cache directory path. */
    int persistent; /**< Whether persistent caching is enabled. */
};

/**
 * @brief Pushes a chunk request onto the queue.
 *
 * @param m Pointer to the stream manager.
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @return 1 on success, 0 on failure.
 */
static int queue_push(struct RogueChunkStreamManager* m, int cx, int cy)
{
    if (m->q_count >= ROGUE_STREAM_MAX_QUEUE)
        return 0;
    /* avoid duplicate in queue */
    for (int i = 0; i < m->q_count; i++)
    {
        int idx = (m->q_head + i) % ROGUE_STREAM_MAX_QUEUE;
        if (m->queue[idx].cx == cx && m->queue[idx].cy == cy)
            return 1;
    }
    int pos = (m->q_tail) % ROGUE_STREAM_MAX_QUEUE;
    m->queue[pos].cx = cx;
    m->queue[pos].cy = cy;
    m->q_tail = (m->q_tail + 1) % ROGUE_STREAM_MAX_QUEUE;
    m->q_count++;
    return 1;
}

/**
 * @brief Pops a chunk request from the queue.
 *
 * @param m Pointer to the stream manager.
 * @param out_cx Pointer to output chunk X coordinate.
 * @param out_cy Pointer to output chunk Y coordinate.
 * @return 1 on success, 0 if queue is empty.
 */
static int queue_pop(struct RogueChunkStreamManager* m, int* out_cx, int* out_cy)
{
    if (m->q_count == 0)
        return 0;
    int pos = m->q_head;
    *out_cx = m->queue[pos].cx;
    *out_cy = m->queue[pos].cy;
    m->q_head = (m->q_head + 1) % ROGUE_STREAM_MAX_QUEUE;
    m->q_count--;
    return 1;
}

/**
 * @brief Allocates a new chunk.
 *
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @return Pointer to the allocated chunk, or NULL on failure.
 */
static struct RogueGeneratedChunk* alloc_chunk(int cx, int cy)
{
    struct RogueGeneratedChunk* c = (struct RogueGeneratedChunk*) malloc(sizeof *c);
    if (!c)
        return NULL;
    c->cx = cx;
    c->cy = cy;
    c->hash = 0;
    c->last_access_tick = 0;
    if (!rogue_tilemap_init(&c->map, ROGUE_WORLD_CHUNK_SIZE, ROGUE_WORLD_CHUNK_SIZE))
    {
        free(c);
        return NULL;
    }
    return c;
}

/**
 * @brief Frees a chunk.
 *
 * @param c Pointer to the chunk to free.
 */
static void free_chunk(struct RogueGeneratedChunk* c)
{
    if (!c)
        return;
    rogue_tilemap_free(&c->map);
    free(c);
}

/**
 * @brief Computes the hash of a chunk's tiles.
 *
 * @param m Pointer to the tile map.
 * @return Hash value.
 */
static unsigned long long hash_chunk_tiles(const RogueTileMap* m)
{
    return rogue_world_hash_tilemap(m);
}

/**
 * @brief Generates a chunk.
 *
 * @param m Pointer to the stream manager.
 * @param c Pointer to the chunk to generate.
 */
static void generate_chunk(struct RogueChunkStreamManager* m, struct RogueGeneratedChunk* c)
{
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &m->base_cfg);
    /* Derive per-chunk seed: base seed xor with chunk coords to keep determinism & isolation */
    RogueWorldGenConfig tmp = m->base_cfg;
    tmp.seed = m->base_cfg.seed ^
               ((unsigned int) (c->cx * 73856093u) ^ (unsigned int) (c->cy * 19349663u));
    RogueTileMap* map = &c->map;
    /* For streaming slice we run macro layout on a chunk-locally sized map only (simplified) */
    rogue_world_generate_macro_layout(&tmp, &ctx, map, NULL, NULL);
    c->hash = hash_chunk_tiles(map);
    rogue_worldgen_context_shutdown(&ctx);
}

/**
 * @brief Creates a new chunk stream manager.
 *
 * @param base_cfg Pointer to the base world generation config.
 * @param budget_per_tick Processing budget per tick.
 * @param capacity Cache capacity.
 * @param cache_dir Cache directory path.
 * @param enable_persistent_cache Whether to enable persistent caching.
 * @return Pointer to the created manager, or NULL on failure.
 */
RogueChunkStreamManager* rogue_chunk_stream_create(const RogueWorldGenConfig* base_cfg,
                                                   int budget_per_tick, int capacity,
                                                   const char* cache_dir,
                                                   int enable_persistent_cache)
{
    if (!base_cfg || capacity <= 0)
        return NULL;
    struct RogueChunkStreamManager* m = (struct RogueChunkStreamManager*) calloc(1, sizeof *m);
    if (!m)
        return NULL;
    m->base_cfg = *base_cfg;
    m->budget_per_tick = budget_per_tick > 0 ? budget_per_tick : 1;
    m->capacity = capacity;
    m->entries = (RogueChunkCacheEntry*) calloc(capacity, sizeof(RogueChunkCacheEntry));
    if (!m->entries)
    {
        free(m);
        return NULL;
    }
    m->global_tick = 0;
    m->persistent = enable_persistent_cache ? 1 : 0;
    if (cache_dir)
    {
        size_t i = 0;
        for (; i < sizeof m->cache_dir - 1 && cache_dir[i]; ++i)
            m->cache_dir[i] = cache_dir[i];
        m->cache_dir[i] = '\0';
    }
    return m;
}

/**
 * @brief Destroys a chunk stream manager.
 *
 * @param m Pointer to the manager to destroy.
 */
void rogue_chunk_stream_destroy(RogueChunkStreamManager* m)
{
    if (!m)
        return;
    for (int i = 0; i < m->capacity; i++)
    {
        if (m->entries[i].in_use)
        {
            free_chunk(m->entries[i].chunk);
        }
    }
    free(m->entries);
    free(m);
}

/**
 * @brief Finds the index of a chunk in the cache.
 *
 * @param m Pointer to the stream manager.
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @return Index of the chunk, or -1 if not found.
 */
static int find_chunk_index(struct RogueChunkStreamManager* m, int cx, int cy)
{
    for (int i = 0; i < m->capacity; i++)
    {
        if (m->entries[i].in_use && m->entries[i].chunk->cx == cx && m->entries[i].chunk->cy == cy)
            return i;
    }
    return -1;
}

/**
 * @brief Enqueues a chunk for generation.
 *
 * @param mgr Pointer to the stream manager.
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @return 1 on success, 0 on failure.
 */
int rogue_chunk_stream_enqueue(RogueChunkStreamManager* mgr, int cx, int cy)
{
    if (!mgr)
        return 0;
    if (find_chunk_index(mgr, cx, cy) >= 0)
        return 1;
    return queue_push(mgr, cx, cy);
}

/**
 * @brief Finds the index of the chunk to evict using LRU.
 *
 * @param m Pointer to the stream manager.
 * @return Index of the chunk to evict, or -1 if none.
 */
static int lru_evict_index(struct RogueChunkStreamManager* m)
{
    unsigned long oldest_tick = ~0u;
    int oldest = -1;
    for (int i = 0; i < m->capacity; i++)
    {
        if (m->entries[i].in_use)
        {
            if (m->entries[i].chunk->last_access_tick <= oldest_tick)
            {
                oldest_tick = m->entries[i].chunk->last_access_tick;
                oldest = i;
            }
        }
        else
        {
            return i;
        }
    }
    return oldest;
}

/**
 * @brief Updates the stream manager, processing queued chunks.
 *
 * @param mgr Pointer to the stream manager.
 * @return Number of chunks processed.
 */
int rogue_chunk_stream_update(RogueChunkStreamManager* mgr)
{
    if (!mgr)
        return 0;
    int processed = 0;
    mgr->global_tick++;
    while (processed < mgr->budget_per_tick)
    {
        int cx, cy;
        if (!queue_pop(mgr, &cx, &cy))
            break; /* already loaded? */
        if (find_chunk_index(mgr, cx, cy) >= 0)
            continue;
        int idx = lru_evict_index(mgr);
        if (idx < 0)
            break;
        if (mgr->entries[idx].in_use)
        {
            mgr->stats.evictions++;
            free_chunk(mgr->entries[idx].chunk);
            mgr->entries[idx].in_use = 0;
            mgr->loaded--;
        }
        struct RogueGeneratedChunk* c = alloc_chunk(cx, cy);
        if (!c)
            continue;
        generate_chunk(mgr, c);
        mgr->entries[idx].chunk = c;
        mgr->entries[idx].in_use = 1;
        mgr->loaded++;
        processed++;
    }
    return processed;
}

/**
 * @brief Retrieves a chunk from the cache.
 *
 * @param mgr Pointer to the stream manager.
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @return Pointer to the chunk, or NULL if not found.
 */
const RogueGeneratedChunk* rogue_chunk_stream_get(const RogueChunkStreamManager* mgr, int cx,
                                                  int cy)
{
    if (!mgr)
        return NULL;
    int idx = find_chunk_index((struct RogueChunkStreamManager*) mgr, cx, cy);
    if (idx < 0)
        return NULL;
    mgr->entries[idx].chunk->last_access_tick = mgr->global_tick;
    return mgr->entries[idx].chunk;
}

/**
 * @brief Requests a chunk, enqueuing if not cached.
 *
 * @param mgr Pointer to the stream manager.
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @return 1 if available or enqueued, 0 on failure.
 */
int rogue_chunk_stream_request(RogueChunkStreamManager* mgr, int cx, int cy)
{
    if (!mgr)
        return 0;
    if (find_chunk_index(mgr, cx, cy) >= 0)
    {
        mgr->stats.cache_hits++;
        return 1;
    }
    mgr->stats.cache_misses++;
    return rogue_chunk_stream_enqueue(mgr, cx, cy);
}

/**
 * @brief Gets the current statistics.
 *
 * @param mgr Pointer to the stream manager.
 * @return Statistics structure.
 */
RogueChunkStreamStats rogue_chunk_stream_get_stats(const RogueChunkStreamManager* mgr)
{
    RogueChunkStreamStats s = {0, 0, 0};
    if (!mgr)
        return s;
    return mgr->stats;
}

/**
 * @brief Gets the number of loaded chunks.
 *
 * @param mgr Pointer to the stream manager.
 * @return Number of loaded chunks.
 */
int rogue_chunk_stream_loaded_count(const RogueChunkStreamManager* mgr)
{
    if (!mgr)
        return 0;
    return mgr->loaded;
}

/**
 * @brief Gets the hash of a chunk.
 *
 * @param mgr Pointer to the stream manager.
 * @param cx Chunk X coordinate.
 * @param cy Chunk Y coordinate.
 * @param out_hash Pointer to output hash.
 * @return 1 on success, 0 on failure.
 */
int rogue_chunk_stream_chunk_hash(const RogueChunkStreamManager* mgr, int cx, int cy,
                                  unsigned long long* out_hash)
{
    if (!mgr || !out_hash)
        return 0;
    int idx = find_chunk_index((struct RogueChunkStreamManager*) mgr, cx, cy);
    if (idx < 0)
        return 0;
    *out_hash = mgr->entries[idx].chunk->hash;
    return 1;
}
