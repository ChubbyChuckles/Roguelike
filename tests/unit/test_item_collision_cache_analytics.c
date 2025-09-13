/* test_item_collision_cache_analytics.c - Validate deterministic analytics snapshot */
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <assert.h>
#include <stdio.h>

static int handle_is_weapon(RogueItemDefHandle h)
{
    int idx = rogue_item_def_index_from_handle(h);
    if (idx < 0)
        return 0;
    const RogueItemDef* d = rogue_item_def_at(idx);
    return d && d->category == ROGUE_ITEM_WEAPON;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();

    int total = rogue_item_defs_count();
    if (total < 3)
    {
        printf("[skip] insufficient items for analytics snapshot test\n");
        return 0;
    }

    /* Find three weapon items to exercise MRU ordering */
    int found[3] = {-1, -1, -1};
    int f = 0;
    for (int i = 0; i < total && f < 3; ++i)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_WEAPON)
            found[f++] = i;
    }
    if (f < 3)
    {
        printf("[skip] not enough weapon items for analytics snapshot test\n");
        return 0;
    }

    RogueItemDefHandle h0 = rogue_item_def_handle_from_index(found[0]);
    RogueItemDefHandle h1 = rogue_item_def_handle_from_index(found[1]);
    RogueItemDefHandle h2 = rogue_item_def_handle_from_index(found[2]);

    /* Populate cache by accessing h0, h1, then h2, and once more h1 to make it MRU */
    assert(rogue_item_collision_cache_get(h0, &cfg));
    assert(rogue_item_collision_cache_get(h1, &cfg));
    assert(rogue_item_collision_cache_get(h2, &cfg));
    assert(rogue_item_collision_cache_get(h1, &cfg));

    /* Snapshot and verify order: expected MRU is h1, then h2 or h0 depending on access ticks. */
    RogueItemCollisionCacheEntryInfo infos[8];
    size_t count = rogue_item_collision_cache_snapshot(infos, 8);
    assert(count >= 3);

    /* Check that the first is h1 (MRU), that all handles are weapons, and approx_bytes>0. */
    assert(infos[0].handle == h1);
    assert(handle_is_weapon(infos[0].handle));
    assert(infos[0].approx_bytes > 0);

    /* Access counts should be non-zero and ticks should be non-decreasing when walking MRU->LRU. */
    for (size_t i = 0; i + 1 < count; ++i)
    {
        assert(infos[i].access_count > 0);
        assert(infos[i + 1].access_count > 0);
        /* last_access_tick should be monotonically decreasing from MRU to LRU (strictly or equal)
         */
        assert(infos[i].last_access_tick >= infos[i + 1].last_access_tick);
    }

    /* Count-only query should match count. */
    size_t count2 = rogue_item_collision_cache_snapshot(NULL, 0);
    assert(count2 == count);

    printf("analytics snapshot ok: count=%zu, MRU handle=%u, bytes0=%zu\n", count,
           (unsigned) infos[0].handle, infos[0].approx_bytes);
    return 0;
}
