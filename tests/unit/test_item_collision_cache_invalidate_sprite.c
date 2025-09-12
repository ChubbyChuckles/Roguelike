/* test_item_collision_cache_invalidate_sprite.c - Invalidate by sprite path */
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_two_groups(void)
{
    int base = rogue_item_defs_count();
    struct RogueItemDef d;
    memset(&d, 0, sizeof(d));
    // Group A
    for (int i = 0; i < 2; ++i)
    {
        memset(&d, 0, sizeof(d));
        snprintf(d.id, sizeof(d.id), "grpA_%d", base + i);
        snprintf(d.name, sizeof(d.name), "GroupA %d", base + i);
        d.category = ROGUE_ITEM_WEAPON;
        snprintf(d.sprite_sheet, sizeof(d.sprite_sheet), "assets/placeholder.png");
        d.sprite_tw = d.sprite_th = 8;
        if (rogue_item_defs_add(&d) < 0)
            return 0;
    }
    // Group B
    for (int i = 0; i < 2; ++i)
    {
        memset(&d, 0, sizeof(d));
        snprintf(d.id, sizeof(d.id), "grpB_%d", base + 2 + i);
        snprintf(d.name, sizeof(d.name), "GroupB %d", base + 2 + i);
        d.category = ROGUE_ITEM_WEAPON;
        snprintf(d.sprite_sheet, sizeof(d.sprite_sheet), "assets/different.png");
        d.sprite_tw = d.sprite_th = 8;
        if (rogue_item_defs_add(&d) < 0)
            return 0;
    }
    return 1;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    if (!add_two_groups())
    {
        fprintf(stderr, "Failed to add items\n");
        return 1;
    }
    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();

    int base = rogue_item_defs_count() - 4;
    RogueItemDefHandle a0 = rogue_item_def_handle_from_index(base + 0);
    RogueItemDefHandle a1 = rogue_item_def_handle_from_index(base + 1);
    RogueItemDefHandle b0 = rogue_item_def_handle_from_index(base + 2);
    RogueItemDefHandle b1 = rogue_item_def_handle_from_index(base + 3);

    (void) rogue_item_collision_cache_get(a0, &cfg);
    (void) rogue_item_collision_cache_get(a1, &cfg);
    (void) rogue_item_collision_cache_get(b0, &cfg);
    (void) rogue_item_collision_cache_get(b1, &cfg);

    // Invalidate sprite for group A only
    rogue_item_collision_cache_invalidate_sprite("assets/placeholder.png");

    int ra0 = rogue_item_collision_cache_is_ready(a0);
    int ra1 = rogue_item_collision_cache_is_ready(a1);
    int rb0 = rogue_item_collision_cache_is_ready(b0);
    int rb1 = rogue_item_collision_cache_is_ready(b1);

    if (ra0 || ra1)
    {
        fprintf(stderr, "Group A should have been invalidated (ra0=%d, ra1=%d)\n", ra0, ra1);
        return 1;
    }
    if (!rb0 || !rb1)
    {
        fprintf(stderr, "Group B should remain cached (rb0=%d, rb1=%d)\n", rb0, rb1);
        return 1;
    }

    return 0;
}
