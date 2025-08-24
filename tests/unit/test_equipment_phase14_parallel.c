/* Phase 14.4: Parallel (async) loadout optimization & arena integration tests */
#define SDL_MAIN_HANDLED 1
#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_perf.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loadout_optimizer.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/stat_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_item_defs_load_from_cfg(const char* path);
int rogue_minimap_ping_loot(float x, float y, int rarity)
{
    (void) x;
    (void) y;
    (void) rarity;
    return 0;
}

static void ensure_item_content(void)
{
    if (rogue_item_defs_count() > 0)
        return;
    int added = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
    assert(added > 0);
}

static void spawn_varied_weapons(int count)
{
    int total = rogue_item_defs_count();
    int spawned = 0;
    for (int i = 0; i < total && spawned < count; i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        if (d->category == ROGUE_ITEM_WEAPON)
        {
            rogue_items_spawn(i, 1, 0, 0);
            spawned++;
        }
    }
}

static void test_async_optimizer(void)
{
    ensure_item_content();
    rogue_equip_reset();
    rogue_loadout_cache_reset();
    spawn_varied_weapons(6); /* equip first weapon */
    int first = -1;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (it)
        {
            const RogueItemDef* d = rogue_item_def_at(it->def_index);
            if (d && d->category == ROGUE_ITEM_WEAPON)
            {
                first = i;
                break;
            }
        }
    }
    assert(first >= 0);
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, first) == 0);
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_force_update(&g_exposed_player_for_stats);
    int base_dps = g_player_stat_cache.dps_estimate;
    int base_ehp = g_player_stat_cache.ehp_estimate;
    int launched = rogue_loadout_optimize_async(50, base_ehp - 10);
    assert(launched == 0); /* wait */
    int guard = 0;
    while (rogue_loadout_optimize_async_running() && guard < 1000000)
    { /* spin-wait tiny - typically immediate */
        guard++;
    }
    int res = rogue_loadout_optimize_join();
    assert(res >= 0);
    assert(g_player_stat_cache.dps_estimate >= base_dps);
}

static void test_arena_reuse(void)
{
    rogue_equip_frame_reset();
    size_t before_high =
        rogue_equip_frame_high_water(); /* run optimizer twice: allocations should not exceed
                                           capacity and second run should not leak */
    int baseline = rogue_loadout_optimize(0, 0);
    (void) baseline;
    size_t after_first = rogue_equip_frame_high_water();
    assert(after_first <= rogue_equip_frame_capacity());
    int second = rogue_loadout_optimize(0, 0);
    (void) second;
    size_t after_second =
        rogue_equip_frame_high_water(); /* high water may grow first run, but second should not
                                           explode and must stay <= capacity */
    assert(after_second <= rogue_equip_frame_capacity());
}

int main(void)
{
    rogue_items_init_runtime();
    test_async_optimizer();
    test_arena_reuse();
    printf("equipment_phase14_parallel_ok\n");
    return 0;
}
