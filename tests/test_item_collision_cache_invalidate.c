#include "game/hit_pixel_mask.h"
#include "game/item_collision_cache.h"
#include "game/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

/* Simple test verifying invalidate_handle + invalidate_all mechanics.
   Assumptions: At least one weapon item definition exists at index 0. */

static RogueItemDefHandle first_weapon_handle(void)
{
    int count = rogue_item_def_count();
    for (int i = 0; i < count; ++i)
    {
        const RogueItemDef* def = rogue_item_def_from_index(i);
        if (def && def->category == ROGUE_ITEM_WEAPON)
        {
            return rogue_item_def_handle_from_index(i);
        }
    }
    return ROGUE_ITEM_DEF_INVALID_HANDLE;
}

int test_item_collision_cache_invalidate(void)
{
    rogue_item_collision_cache_reset();

    RogueItemDefHandle h = first_weapon_handle();
    assert(h != ROGUE_ITEM_DEF_INVALID_HANDLE && "Need at least one weapon def for test");

    RoguePixelMaskLoadConfig cfg = {0};
    RoguePixelMaskSet* a = rogue_item_collision_cache_get(h, &cfg);
    assert(a);

    RogueItemCollisionCacheStats st1 = rogue_item_collision_cache_get_stats();
    assert(st1.lookups == 1);
    assert(st1.hits == 0);
    assert(st1.misses == 1); /* first build is a miss */

    RoguePixelMaskSet* b = rogue_item_collision_cache_get(h, &cfg);
    assert(b == a);
    RogueItemCollisionCacheStats st2 = rogue_item_collision_cache_get_stats();
    assert(st2.hits == 1);

    rogue_item_collision_cache_invalidate_handle(h);
    RogueItemCollisionCacheStats st3 = rogue_item_collision_cache_get_stats();
    assert(st3.invalidations == 1);

    RoguePixelMaskSet* c = rogue_item_collision_cache_get(h, &cfg);
    assert(c);
    assert(c != a); /* entry rebuilt */

    rogue_item_collision_cache_invalidate_all();
    RogueItemCollisionCacheStats st4 = rogue_item_collision_cache_get_stats();
    assert(st4.invalidations >= 2);

    return 0;
}
