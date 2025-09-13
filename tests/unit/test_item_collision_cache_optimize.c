/* test_item_collision_cache_optimize.c - Validate cache optimize() compacts and preserves data */
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* Item defs are usually preloaded by the test harness; if not available, skip. */
    rogue_item_collision_cache_reset();

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();

    /* Pick two consecutive weapon items if available */
    int total = rogue_item_defs_count();
    if (total < 2)
    {
        printf("[skip] not enough items to test optimize\n");
        return 0; /* treat as pass when unavailable */
    }
    int first = -1, second = -1;
    for (int i = 0; i < total - 1; ++i)
    {
        const RogueItemDef* d0 = rogue_item_def_at(i);
        const RogueItemDef* d1 = rogue_item_def_at(i + 1);
        if (d0 && d1 && d0->category == ROGUE_ITEM_WEAPON && d1->category == ROGUE_ITEM_WEAPON)
        {
            first = i;
            second = i + 1;
            break;
        }
    }
    if (first < 0 || second < 0)
    {
        printf("[skip] no consecutive weapon items found\n");
        return 0;
    }

    RogueItemDefHandle h0 = rogue_item_def_handle_from_index(first);
    RogueItemDefHandle h1 = rogue_item_def_handle_from_index(second);

    RoguePixelMaskSet* s0 = rogue_item_collision_cache_get(h0, &cfg);
    RoguePixelMaskSet* s1 = rogue_item_collision_cache_get(h1, &cfg);
    assert(s0 != NULL && s1 != NULL);

    /* Invalidate one to create a tombstone */
    rogue_item_collision_cache_invalidate_handle(h0);

    /* Before optimize, h1 should be ready, h0 not. */
    assert(rogue_item_collision_cache_is_ready(h0) == 0);
    assert(rogue_item_collision_cache_is_ready(h1) == 1);

    /* Run optimize to compact and recompute stats */
    rogue_item_collision_cache_optimize();

    /* After optimize, readiness must be unchanged */
    assert(rogue_item_collision_cache_is_ready(h0) == 0);
    assert(rogue_item_collision_cache_is_ready(h1) == 1);

    /* Stats sanity: approx_bytes should be > 0 when one set remains */
    RogueItemCollisionCacheStats st = rogue_item_collision_cache_get_stats();
    assert(st.approx_bytes > 0);

    printf("optimize ok: approx_bytes=%zu, lookups=%llu, hits=%llu, misses=%llu, evictions=%llu, "
           "invalidations=%llu\n",
           (size_t) st.approx_bytes, (unsigned long long) st.lookups, (unsigned long long) st.hits,
           (unsigned long long) st.misses, (unsigned long long) st.evictions,
           (unsigned long long) st.invalidations);
    return 0;
}
