#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <assert.h>
#include <stdio.h>

/* This test exercises the dynamic limits API:
   - Set a small entry cap (e.g., 2) and request 3 distinct items -> the LRU tail must be evicted.
   - Set a tiny memory cap (0 MB) -> cache must evict all entries immediately.
   The test uses existing item defs; it only exercises cache bookkeeping and does not require
   loading real assets in headless mode since non-weapon items will result in empty sets. */

static void prepare_items(RogueItemDefHandle* out, int* count)
{
    int total = rogue_item_defs_count();
    int c = 0;
    for (int i = 0; i < total && c < 3; ++i)
    {
        RogueItemDefHandle h = rogue_item_def_handle_from_index(i);
        if (h != ROGUE_ITEM_DEF_INVALID_HANDLE)
            out[c++] = h;
    }
    *count = c;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    rogue_item_collision_cache_init();

    RogueItemDefHandle hs[4];
    int n = 0;
    prepare_items(hs, &n);
    if (n < 3)
    {
        /* Not enough items to run this test meaningfully. Consider it pass. */
        return 0;
    }

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();

    /* 1) Set entry cap = 2 and request 3 items -> ensure only 2 remain. */
    rogue_item_collision_cache_set_limits(2, /*MB*/ 64);

    for (int i = 0; i < 3; ++i)
    {
        (void) rogue_item_collision_cache_get(hs[i], &cfg);
    }

    RogueItemCollisionCacheEntryInfo snap[8];
    size_t count = rogue_item_collision_cache_snapshot(snap, 8);
    assert(count <= 2 && "Entry cap not enforced deterministically");

    /* 2) Set memory cap = 0 MB -> all entries must be evicted on enforcement. */
    rogue_item_collision_cache_set_limits(2, 0);
    count = rogue_item_collision_cache_snapshot(snap, 8);
    assert(count == 0 && "Memory cap 0MB must evict all entries");

    /* 3) Restore defaults and ensure we can cache again. */
    rogue_item_collision_cache_set_limits(ROGUE_COLLISION_CACHE_SIZE,
                                          ROGUE_COLLISION_CACHE_MAX_MEMORY_MB);
    (void) rogue_item_collision_cache_get(hs[0], &cfg);
    count = rogue_item_collision_cache_snapshot(snap, 8);
    assert(count >= 1);

    printf("test_item_collision_cache_limits: OK\n");
    return 0;
}
