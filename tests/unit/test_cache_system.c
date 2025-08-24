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
    assert(rogue_cache_init(0, 0, 0) == 0);
    // Basic put/get + promotion
    const char msg[] = "hello cache";
    assert(rogue_cache_put(0xAULL, msg, sizeof(msg), 1, -1) == 0);
    void* data = NULL;
    size_t sz = 0;
    uint32_t ver = 0;
    assert(rogue_cache_get(0xAULL, &data, &sz, &ver) == 1);
    assert(sz == sizeof(msg));
    assert(ver == 1);
    // Miss
    assert(rogue_cache_get(0xBULL, &data, &sz, &ver) == 0);
    // Add large entry to hit L2/L3 path
    unsigned char big[5000];
    memset(big, 0xCD, sizeof(big));
    assert(rogue_cache_put(0xCULL, big, sizeof(big), 2, -1) == 0);
    assert(rogue_cache_get(0xCULL, &data, &sz, &ver) == 1);
    assert(sz == sizeof(big));
    // Preload several keys
    uint64_t keys[5] = {100, 101, 102, 103, 104};
    int loaded = rogue_cache_preload(keys, 5, ROGUE_CACHE_L2, loader_cb);
    assert(loaded == 5);
    // Iterate
    int count = 0;
    rogue_cache_iterate(iter_cb, &count);
    assert(count >= 7);
    // Invalidate one
    rogue_cache_invalidate(0xAULL);
    assert(rogue_cache_get(0xAULL, &data, &sz, &ver) == 0);
    // Invalidate all
    rogue_cache_invalidate_all();
    assert(rogue_cache_get(0xCULL, &data, &sz, &ver) == 0);
    // Compression threshold test: store a repetitive big buffer
    rogue_cache_set_compress_threshold(64);
    unsigned char rep[256];
    memset(rep, 0x11, sizeof(rep));
    assert(rogue_cache_put(0xDEAD, rep, sizeof(rep), 5, -1) == 0);
    assert(rogue_cache_get(0xDEAD, &data, &sz, &ver) == 1);
    RogueCacheStats stats;
    rogue_cache_get_stats(&stats);
    assert(stats.compressed_entries >= 1);
    rogue_cache_dump();
    rogue_cache_shutdown();
    printf("test_cache_system OK\n");
    return 0;
}
