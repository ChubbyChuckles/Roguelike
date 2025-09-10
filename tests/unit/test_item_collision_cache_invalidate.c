/* test_item_collision_cache_invalidate.c - Verifies handle + global invalidation.
 */
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int ensure_weapon_items(int n)
{
    int count = rogue_item_defs_count();
    if (count >= n)
        return 1;
    for (int i = count; i < n; ++i)
    {
        struct RogueItemDef d;
        memset(&d, 0, sizeof(d));
        snprintf(d.id, sizeof(d.id), "test_weapon_%d", i);
        snprintf(d.name, sizeof(d.name), "Test Weapon %d", i);
        d.category = ROGUE_ITEM_WEAPON;
        d.stack_max = 1;
        d.base_value = 1;
        d.base_damage_min = 1;
        d.base_damage_max = 2;
        d.sprite_tw = d.sprite_th = 8;
        if (rogue_item_defs_add(&d) < 0)
            return 0;
    }
    return 1;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    if (!ensure_weapon_items(1))
    {
        fprintf(stderr, "Failed to ensure weapon item\n");
        return 1;
    }
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.generate_distance_fields = 0;

    RogueItemDefHandle h = rogue_item_def_handle_from_index(0);
    RoguePixelMaskSet* first = rogue_item_collision_cache_get(h, &cfg);
    if (!first)
    {
        fprintf(stderr, "Initial build failed\n");
        return 1;
    }
    RogueItemCollisionCacheStats s1 = rogue_item_collision_cache_get_stats();
    if (s1.misses != 1 || s1.hits != 0)
    {
        fprintf(stderr, "Unexpected stats after first get (misses=%llu hits=%llu)\n",
                (unsigned long long) s1.misses, (unsigned long long) s1.hits);
        return 1;
    }

    /* Hit */
    RoguePixelMaskSet* second = rogue_item_collision_cache_get(h, &cfg);
    if (second != first)
    {
        fprintf(stderr, "Expected pointer identity on hit\n");
        return 1;
    }
    RogueItemCollisionCacheStats s2 = rogue_item_collision_cache_get_stats();
    if (s2.hits != 1)
    {
        fprintf(stderr, "Expected one hit after second lookup (hits=%llu)\n",
                (unsigned long long) s2.hits);
        return 1;
    }

    rogue_item_collision_cache_invalidate_handle(h);
    RogueItemCollisionCacheStats s3 = rogue_item_collision_cache_get_stats();
    if (s3.invalidations < 1)
    {
        fprintf(stderr, "Expected invalidations >=1 after handle invalidation\n");
        return 1;
    }

    RoguePixelMaskSet* third = rogue_item_collision_cache_get(h, &cfg);
    if (!third)
    {
        fprintf(stderr, "Rebuild after invalidation failed\n");
        return 1;
    }
    RogueItemCollisionCacheStats s3b = rogue_item_collision_cache_get_stats();
    if (s3b.misses < s3.misses + 1)
    {
        fprintf(stderr, "Expected a new miss after invalidation rebuild (misses now %llu)\n",
                (unsigned long long) s3b.misses);
        return 1;
    }

    rogue_item_collision_cache_invalidate_all();
    RogueItemCollisionCacheStats s4 = rogue_item_collision_cache_get_stats();
    if (s4.invalidations < 2)
    {
        fprintf(stderr, "Expected invalidations >=2 after global invalidation (got %llu)\n",
                (unsigned long long) s4.invalidations);
        return 1;
    }

    return 0;
}
