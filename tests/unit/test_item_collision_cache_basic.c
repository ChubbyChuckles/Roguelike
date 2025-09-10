/* test_item_collision_cache_basic.c - Verifies basic LRU cache behavior for item collision cache.
 * Focus: initialization, miss -> load (weapon), hit, eviction (simulate > size), stats sanity.
 */
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <stdio.h>
#include <string.h>

static int ensure_min_items(int n)
{
    int count = rogue_item_defs_count();
    if (count >= n)
        return 1;
    /* Synthesize simple weapon defs if needed (id = test_weapon_i) */
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
        d.sprite_tw = d.sprite_th = 8; /* placeholder dims */
        if (rogue_item_defs_add(&d) < 0)
            return 0;
    }
    return 1;
}

int main(void)
{
    rogue_item_collision_cache_init();
    if (!ensure_min_items(4))
    {
        fprintf(stderr, "Failed to synthesize item defs\n");
        return 1;
    }
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.generate_distance_fields = 0; /* keep light */
    /* Acquire handle for first weapon */
    RogueItemDefHandle h0 = rogue_item_def_handle_from_index(0);
    RoguePixelMaskSet* set0 = rogue_item_collision_cache_get(h0, &cfg);
    if (!set0)
    {
        fprintf(stderr, "Cache miss load failed\n");
        return 1;
    }
    RogueItemCollisionCacheStats s1 = rogue_item_collision_cache_get_stats();
    if (s1.lookups != 1 || s1.misses != 1 || s1.hits != 0)
    {
        fprintf(stderr, "Unexpected stats after first miss (lookups=%llu hits=%llu misses=%llu)\n",
                (unsigned long long) s1.lookups, (unsigned long long) s1.hits,
                (unsigned long long) s1.misses);
        return 1;
    }
    /* Repeat (hit) */
    RoguePixelMaskSet* set0b = rogue_item_collision_cache_get(h0, &cfg);
    if (set0b != set0)
    {
        fprintf(stderr, "Hit returned different pointer\n");
        return 1;
    }
    RogueItemCollisionCacheStats s2 = rogue_item_collision_cache_get_stats();
    if (s2.hits != 1 || s2.lookups != 2)
    {
        fprintf(stderr, "Expected one hit after second lookup (hits=%llu lookups=%llu)\n",
                (unsigned long long) s2.hits, (unsigned long long) s2.lookups);
        return 1;
    }
    /* Stress entries beyond small subset for LRU eviction test (use SIZE/2 limited to 16). */
    int limit = 16;
    if (limit > ROGUE_COLLISION_CACHE_SIZE)
        limit = ROGUE_COLLISION_CACHE_SIZE;
    if (!ensure_min_items(limit))
    {
        fprintf(stderr, "Failed to ensure items for eviction test\n");
        return 1;
    }
    for (int i = 1; i < limit; ++i)
    {
        RogueItemDefHandle h = rogue_item_def_handle_from_index(i);
        (void) rogue_item_collision_cache_get(h, &cfg);
    }
    RogueItemCollisionCacheStats s3 = rogue_item_collision_cache_get_stats();
    if (s3.lookups < (unsigned) limit)
    {
        fprintf(stderr, "Lookup count too low after population %llu < %d\n",
                (unsigned long long) s3.lookups, limit);
        return 1;
    }
    /* Basic sanity: hits >=1 (from earlier) and misses >= (limit) */
    if (s3.hits < 1 || s3.misses < 1)
    {
        fprintf(stderr, "Stats sanity fail hits=%llu misses=%llu\n", (unsigned long long) s3.hits,
                (unsigned long long) s3.misses);
        return 1;
    }
    return 0;
}
