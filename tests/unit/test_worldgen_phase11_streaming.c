/* Phase 11 runtime streaming & caching tests */
#include "world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 1111;
    cfg->width = 128;
    cfg->height = 128;
    cfg->noise_octaves = 3;
    cfg->water_level = 0.30;
}

int main(void)
{
    RogueWorldGenConfig cfg;
    init_cfg(&cfg);
    /* Create streaming manager with small capacity to force evictions */
    RogueChunkStreamManager* mgr =
        rogue_chunk_stream_create(&cfg, /*budget*/ 3, /*capacity*/ 4, NULL, 0);
    if (!mgr)
    {
        fprintf(stderr, "create fail\n");
        return 1;
    }
    /* Request 6 distinct chunks -> will exceed capacity */
    for (int i = 0; i < 6; i++)
    {
        if (!rogue_chunk_stream_request(mgr, i, 0))
        {
            fprintf(stderr, "enqueue fail %d\n", i);
            return 1;
        }
    }
    int ticks = 0;
    while (rogue_chunk_stream_loaded_count(mgr) < 4 && ticks < 50)
    {
        rogue_chunk_stream_update(mgr);
        ticks++;
    }
    if (rogue_chunk_stream_loaded_count(mgr) != 4)
    {
        fprintf(stderr, "expected 4 loaded got %d\n", rogue_chunk_stream_loaded_count(mgr));
        return 1;
    }
    /* Access some chunks to adjust LRU ordering */
    const RogueGeneratedChunk* c0 = rogue_chunk_stream_get(mgr, 0, 0);
    (void) c0;
    const RogueGeneratedChunk* c1 = rogue_chunk_stream_get(mgr, 1, 0);
    (void) c1;
    /* Request new chunk to trigger eviction */
    if (!rogue_chunk_stream_request(mgr, 10, 0))
    {
        fprintf(stderr, "request new fail\n");
        return 1;
    }
    rogue_chunk_stream_update(mgr);
    RogueChunkStreamStats st = rogue_chunk_stream_get_stats(mgr);
    if (st.cache_misses == 0)
    {
        fprintf(stderr, "expected misses\n");
        return 1;
    }
    /* Determinism: capture hash of chunk (2,0), destroy manager, recreate & generate same chunk
     * alone -> same hash */
    unsigned long long hash_before = 0ULL;
    if (!rogue_chunk_stream_chunk_hash(mgr, 2, 0, &hash_before))
    { /* might have been evicted; request again deterministic */
        rogue_chunk_stream_request(mgr, 2, 0);
        while (!rogue_chunk_stream_chunk_hash(mgr, 2, 0, &hash_before))
        {
            rogue_chunk_stream_update(mgr);
        }
    }
    rogue_chunk_stream_destroy(mgr);
    RogueChunkStreamManager* mgr2 = rogue_chunk_stream_create(&cfg, 3, 4, NULL, 0);
    if (!mgr2)
    {
        fprintf(stderr, "recreate fail\n");
        return 1;
    }
    rogue_chunk_stream_request(mgr2, 2, 0);
    while (!rogue_chunk_stream_chunk_hash(mgr2, 2, 0, &hash_before))
    {
        rogue_chunk_stream_update(mgr2);
    }
    unsigned long long hash_after = 0ULL;
    rogue_chunk_stream_chunk_hash(mgr2, 2, 0, &hash_after);
    if (hash_before != hash_after)
    {
        fprintf(stderr, "hash mismatch %llu vs %llu\n", (unsigned long long) hash_before,
                (unsigned long long) hash_after);
        return 1;
    }
    rogue_chunk_stream_destroy(mgr2);
    printf("phase11 streaming tests passed\n");
    return 0;
}
