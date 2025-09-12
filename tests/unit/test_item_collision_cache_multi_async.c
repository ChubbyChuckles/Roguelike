/* test_item_collision_cache_multi_async.c - Stress multiple async requests and prefetch */
#include "core/integration/thread_pool.h"
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_items_same_sheet(int n, const char* sheet)
{
    int count = rogue_item_defs_count();
    for (int i = 0; i < n; ++i)
    {
        struct RogueItemDef d;
        memset(&d, 0, sizeof(d));
        snprintf(d.id, sizeof(d.id), "multi_item_%d", count + i);
        snprintf(d.name, sizeof(d.name), "Multi Item %d", count + i);
        d.category = ROGUE_ITEM_WEAPON;
        d.stack_max = 1;
        d.base_value = 1;
        d.base_damage_min = 1;
        d.base_damage_max = 2;
        snprintf(d.sprite_sheet, sizeof(d.sprite_sheet), "%s", sheet);
        d.sprite_tw = d.sprite_th = 8;
        if (rogue_item_defs_add(&d) < 0)
            return 0;
    }
    return 1;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    const char* sheet = "assets/placeholder.png";
    if (!add_items_same_sheet(6, sheet))
    {
        fprintf(stderr, "Failed to add items\n");
        return 1;
    }

    RogueThreadPool tp;
    memset(&tp, 0, sizeof(tp));
    if (rogue_thread_pool_init(&tp, 6) != 0)
    {
        fprintf(stderr, "Failed to init pool\n");
        return 1;
    }
    rogue_item_collision_cache_set_thread_pool(&tp);
    rogue_item_collision_cache_set_prefetch(1, 3);

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();

    int total = rogue_item_defs_count();
    int start = total - 6;
    /* submit async for the first three; prefetch should queue up to 3 related */
    for (int i = 0; i < 3; ++i)
    {
        RogueItemDefHandle h = rogue_item_def_handle_from_index(start + i);
        if (!rogue_item_collision_cache_request_async(h, &cfg))
        {
            fprintf(stderr, "queue failed for %d\n", i);
            rogue_thread_pool_shutdown(&tp);
            return 1;
        }
    }

    /* Spin wait for readiness of at least 5 of 6 items, yielding briefly between checks */
    int ready_count = 0;
    for (int tries = 0; tries < 400; ++tries)
    {
        ready_count = 0;
        for (int i = 0; i < 6; ++i)
        {
            RogueItemDefHandle h = rogue_item_def_handle_from_index(start + i);
            if (rogue_item_collision_cache_is_ready(h))
                ready_count++;
        }
        if (ready_count >= 5)
            break;
        /* After an initial grace period, proactively queue remaining items to stress concurrency */
        if (tries == 40)
        {
            for (int i = 3; i < 6; ++i)
            {
                RogueItemDefHandle h = rogue_item_def_handle_from_index(start + i);
                (void) rogue_item_collision_cache_request_async(h, &cfg);
            }
        }
        SDL_Delay(5); /* allow background workers to progress */
    }
    if (ready_count < 5)
    {
        fprintf(stderr, "Expected >=5 items ready via async+prefetch, got %d\n", ready_count);
        rogue_thread_pool_shutdown(&tp);
        return 1;
    }

    rogue_thread_pool_shutdown(&tp);
    return 0;
}
