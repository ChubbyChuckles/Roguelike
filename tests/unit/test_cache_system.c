#include "../../src/core/integration/cache_system.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int loader_cb(uint64_t key, void** out_data, size_t* out_size, uint32_t* out_ver)
{
    // produce deterministic content based on key
    size_t sz = 32 + (key % 17);
    unsigned char* buf = (unsigned char*) malloc(sz);
    for (size_t i = 0; i < sz; i++)
        buf[i] = (unsigned char) (key + i);
    *out_data = buf;
    *out_size = sz;
    *out_ver = (uint32_t) (key & 0xFFFF);
    return 0;
}

static bool iter_cb(uint64_t key, const void* data, size_t size, uint32_t version, int level,
                    void* ud)
{
    (void) ud;
    (void) data;
    (void) version; // just count
    int* count = (int*) ud;
    (*count)++;
    assert(level >= 0 && level < 3);
    assert(size > 0);
    return true;
}

int main(void)
{
    if (rogue_cache_init(0, 0, 0) != 0)
    {
        fprintf(stderr, "cache init failed\n");
        return 1;
    }
    // Basic put/get + promotion
    const char msg[] = "hello cache";
    if (rogue_cache_put(0xAULL, msg, sizeof(msg), 1, -1) != 0)
    {
        fprintf(stderr, "cache put A failed\n");
        return 2;
    }
    void* data = NULL;
    size_t sz = 0;
    uint32_t ver = 0;
    if (rogue_cache_get(0xAULL, &data, &sz, &ver) != 1)
    {
        fprintf(stderr, "cache get A failed\n");
        return 3;
    }
    if (sz != sizeof(msg) || ver != 1)
    {
        fprintf(stderr, "cache A size/ver mismatch sz=%zu ver=%u\n", sz, ver);
        return 4;
    }
    // Miss
    if (rogue_cache_get(0xBULL, &data, &sz, &ver) != 0)
    {
        fprintf(stderr, "cache unexpected hit B\n");
        return 5;
    }
    // Add large entry to hit L2/L3 path
    unsigned char big[5000];
    memset(big, 0xCD, sizeof(big));
    if (rogue_cache_put(0xCULL, big, sizeof(big), 2, -1) != 0)
    {
        fprintf(stderr, "cache put C failed\n");
        return 6;
    }
    if (rogue_cache_get(0xCULL, &data, &sz, &ver) != 1 || sz != sizeof(big))
    {
        fprintf(stderr, "cache get C failed sz=%zu\n", sz);
        return 7;
    }
    // Preload several keys
    uint64_t keys[5] = {100, 101, 102, 103, 104};
    int loaded = rogue_cache_preload(keys, 5, ROGUE_CACHE_L2, loader_cb);
    if (loaded != 5)
    {
        fprintf(stderr, "preload loaded=%d\n", loaded);
        return 8;
    }
    // Iterate
    int count = 0;
    rogue_cache_iterate(iter_cb, &count);
    if (count < 7)
    {
        fprintf(stderr, "iterate count=%d\n", count);
        return 9;
    }
    // Invalidate one
    rogue_cache_invalidate(0xAULL);
    if (rogue_cache_get(0xAULL, &data, &sz, &ver) != 0)
    {
        fprintf(stderr, "invalidate A failed\n");
        return 10;
    }
    // Invalidate all
    rogue_cache_invalidate_all();
    if (rogue_cache_get(0xCULL, &data, &sz, &ver) != 0)
    {
        fprintf(stderr, "invalidate all failed C still present\n");
        return 11;
    }
    // Compression threshold test: store a repetitive big buffer
    rogue_cache_set_compress_threshold(64);
    unsigned char rep[256];
    memset(rep, 0x11, sizeof(rep));
    if (rogue_cache_put(0xDEAD, rep, sizeof(rep), 5, -1) != 0 ||
        rogue_cache_get(0xDEAD, &data, &sz, &ver) != 1)
    {
        fprintf(stderr, "compress put/get failed\n");
        return 12;
    }
    RogueCacheStats stats;
    rogue_cache_get_stats(&stats);
    if (stats.compressed_entries < 1)
    {
        fprintf(stderr, "expected compressed_entries >= 1 got %u\n", stats.compressed_entries);
        return 13;
    }
    rogue_cache_dump();
    rogue_cache_shutdown();
    printf("test_cache_system OK\n");
    return 0;
}
