#include "core/integration/memory_pool.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_fixed_categories(void)
{
// allocate a bunch of tiny allocations to force additional pages
#define TEST_TINY_ALLOC_COUNT 300
    void* ptrs[TEST_TINY_ALLOC_COUNT];
    for (int i = 0; i < TEST_TINY_ALLOC_COUNT; i++)
    {
        ptrs[i] = rogue_mp_alloc(8);
        assert(ptrs[i]);
        memset(ptrs[i], 0xAB, 8);
    }
    for (int i = 0; i < TEST_TINY_ALLOC_COUNT; i++)
    {
        rogue_mp_free(ptrs[i]);
    }
#undef TEST_TINY_ALLOC_COUNT
}

static void test_buddy_large(void)
{
    void* a = rogue_mp_alloc(70000);
    assert(a);
    memset(a, 0, 70000);
    void* b = rogue_mp_alloc(130000);
    assert(b);
    memset(b, 1, 130000);
    rogue_mp_free(a);
    rogue_mp_free(b);
}

static void test_slab(void)
{
    RogueSlabHandle h = rogue_slab_register("TestObj", 40, 16, NULL, NULL);
    assert(h >= 0);
    void* objs[48];
    for (int i = 0; i < 48; i++)
    {
        objs[i] = rogue_slab_alloc(h);
        assert(objs[i]);
        memset(objs[i], (uint8_t) i, 40);
    }
    for (int i = 0; i < 48; i += 2)
    {
        rogue_slab_free(h, objs[i]);
    }
    int freed = rogue_slab_shrink();
    (void) freed; // some fully free pages may be reclaimed
    for (int i = 1; i < 48; i += 2)
    {
        rogue_slab_free(h, objs[i]);
    }
    rogue_slab_shrink();
}

static void test_stats_and_recommendations(void)
{
    RogueMemoryPoolStats s;
    rogue_memory_pool_get_stats(&s);
    assert(s.buddy_total_bytes >= (1 << 16));
    RogueMemoryPoolRecommendation rec;
    rogue_memory_pool_get_recommendations(&rec);
    // no specific asserts beyond structure access; ensure function executes
}

int main(void)
{
    assert(rogue_memory_pool_init(0, false) == 0);
    test_fixed_categories();
    test_buddy_large();
    test_slab();
    test_stats_and_recommendations();
    assert(rogue_memory_pool_validate());
    rogue_memory_pool_dump();
    rogue_memory_pool_shutdown();
    printf("test_memory_pool OK\n");
    return 0;
}
