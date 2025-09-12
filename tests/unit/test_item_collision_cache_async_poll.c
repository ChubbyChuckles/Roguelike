/* test_item_collision_cache_async_poll.c - Validates async request and timestamp polling.
 */
#include "core/integration/thread_pool.h"
#include "core/loot/loot_item_defs.h"
#include "game/item_collision_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int ensure_items(int n)
{
    int count = rogue_item_defs_count();
    if (count >= n)
        return 1;
    for (int i = count; i < n; ++i)
    {
        struct RogueItemDef d;
        memset(&d, 0, sizeof(d));
        snprintf(d.id, sizeof(d.id), "async_item_%d", i);
        snprintf(d.name, sizeof(d.name), "Async Item %d", i);
        d.category = ROGUE_ITEM_WEAPON;
        d.stack_max = 1;
        d.base_value = 1;
        d.base_damage_min = 1;
        d.base_damage_max = 2;
        snprintf(d.sprite_sheet, sizeof(d.sprite_sheet), "assets/placeholder.png");
        d.sprite_tw = d.sprite_th = 8;
        if (rogue_item_defs_add(&d) < 0)
            return 0;
    }
    return 1;
}

/* Deterministic fake mtime that can be advanced by test */
static uint64_t g_fake_mtime = 1000;
static uint64_t fake_mtime_hook(const char* path)
{
    (void) path;
    return g_fake_mtime;
}

int main(void)
{
    rogue_item_collision_cache_reset();
    if (!ensure_items(1))
    {
        fprintf(stderr, "Failed to add items\n");
        return 1;
    }
    /* Register mtime hook */
    rogue_item_collision_cache_set_mtime_hook(fake_mtime_hook);

    /* Create a tiny thread pool for async */
    RogueThreadPool tp;
    memset(&tp, 0, sizeof(tp));
    if (rogue_thread_pool_init(&tp, 2) != 0)
    {
        fprintf(stderr, "Failed to init thread pool\n");
        return 1;
    }
    rogue_item_collision_cache_set_thread_pool(&tp);

    RoguePixelMaskLoadConfig cfg = rogue_pixel_mask_load_config_default();
    cfg.generate_distance_fields = 0;

    RogueItemDefHandle h = rogue_item_def_handle_from_index(0);
    if (!rogue_item_collision_cache_request_async(h, &cfg))
    {
        fprintf(stderr, "Async request failed to queue\n");
        rogue_thread_pool_shutdown(&tp);
        return 1;
    }

    /* Spin-wait briefly for worker to process request */
    const int spins = 1000;
    int ready = 0;
    for (int i = 0; i < spins; ++i)
    {
        if (rogue_item_collision_cache_is_ready(h))
        {
            ready = 1;
            break;
        }
        /* Tiny sleep to yield - use SDL if available, else busy */
    }
    if (!ready)
    {
        fprintf(stderr, "Item not ready after async wait\n");
        rogue_thread_pool_shutdown(&tp);
        return 1;
    }

    /* Poll with unchanged timestamp -> no invalidation */
    int inv0 = rogue_item_collision_cache_poll(4);
    if (inv0 != 0)
    {
        fprintf(stderr, "Expected 0 invalidations, got %d\n", inv0);
        rogue_thread_pool_shutdown(&tp);
        return 1;
    }

    /* Advance fake mtime and poll -> should invalidate >=1 */
    g_fake_mtime += 10;
    int inv1 = rogue_item_collision_cache_poll(4);
    if (inv1 < 1)
    {
        fprintf(stderr, "Expected >=1 invalidation after mtime advance, got %d\n", inv1);
        rogue_thread_pool_shutdown(&tp);
        return 1;
    }

    rogue_thread_pool_shutdown(&tp);
    return 0;
}
