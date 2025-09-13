#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <assert.h>
#include <stdio.h>

static int find_three_weapons(int out[3])
{
    int total = rogue_item_defs_count();
    int found = 0;
    for (int i = 0; i < total && found < 3; ++i)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_WEAPON)
        {
            out[found++] = i;
        }
    }
    return found == 3;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    rogue_item_collision_cache_set_limits_default();

    int idx[3];
    if (!find_three_weapons(idx))
        return 0; /* skip test if not available in this asset set */

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();

    /* Create a deterministic MRU pattern: A, B, C, A, B */
    RogueItemDefHandle ha = rogue_item_def_handle_from_index(idx[0]);
    RogueItemDefHandle hb = rogue_item_def_handle_from_index(idx[1]);
    RogueItemDefHandle hc = rogue_item_def_handle_from_index(idx[2]);

    assert(rogue_item_collision_cache_get(ha, &cfg));
    assert(rogue_item_collision_cache_get(hb, &cfg));
    assert(rogue_item_collision_cache_get(hc, &cfg));
    assert(rogue_item_collision_cache_get(ha, &cfg));
    assert(rogue_item_collision_cache_get(hb, &cfg));

    RogueItemCollisionCacheAdvisory adv1;
    rogue_item_collision_cache_get_advisory(&adv1);

    /* With 3 alive entries, recent_window=min(3,32)=3, recommended=3+1=4 (clamped to size) */
    assert(adv1.alive_entries >= 3);
    assert(adv1.recent_window == 3);
    assert(adv1.recommended_max_entries >= 4 || adv1.recommended_max_entries == 3);
    /* p50/p90 should be non-zero if approx_bytes reported for realized entries */
    assert(adv1.p50_bytes >= 0);
    assert(adv1.p90_bytes >= adv1.p50_bytes);
    /* recommended memory is >=1 when entries exist */
    assert((adv1.alive_entries == 0 && adv1.recommended_max_memory_mb == 0) ||
           adv1.recommended_max_memory_mb >= 1);

    /* Repeat advisory; results must be deterministic for same access pattern */
    RogueItemCollisionCacheAdvisory adv2;
    rogue_item_collision_cache_get_advisory(&adv2);

    assert(adv2.alive_entries == adv1.alive_entries);
    assert(adv2.recent_window == adv1.recent_window);
    assert(adv2.recommended_max_entries == adv1.recommended_max_entries);
    assert(adv2.recommended_max_memory_mb == adv1.recommended_max_memory_mb);
    assert(adv2.p50_bytes == adv1.p50_bytes);
    assert(adv2.p90_bytes == adv1.p90_bytes);
    assert(adv2.p99_bytes == adv1.p99_bytes);

    /* Now cap entries to 2 to change MRU set and recompute; advisory should adapt deterministically
     */
    rogue_item_collision_cache_set_limits(2, adv1.recommended_max_memory_mb);
    RogueItemCollisionCacheAdvisory adv3;
    rogue_item_collision_cache_get_advisory(&adv3);
    assert(adv3.alive_entries <= 2);
    assert(adv3.recent_window == adv3.alive_entries);

    return 0;
}
