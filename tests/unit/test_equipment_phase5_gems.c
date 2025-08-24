#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_gems.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/vendor/economy.h"
#include "../../src/game/stat_cache.h"
#include <assert.h>
#include <stdio.h>

/* Forward declare player struct to satisfy casts */
#include "../../src/entities/player.h"

extern RogueStatCache g_player_stat_cache;

static void reset_player(RoguePlayer* p)
{
    p->strength = 10;
    p->dexterity = 10;
    p->vitality = 10;
    p->intelligence = 10;
    p->crit_chance = 0;
    p->crit_damage = 0;
    p->max_health = 100;
}

int main(void)
{
    extern RogueStatCache
        g_player_stat_cache; /* ensure full reset for isolation under ctest harness */
    memset(&g_player_stat_cache, 0, sizeof g_player_stat_cache);
    rogue_equip_reset();
    rogue_item_defs_reset();
    int base_added = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
    if (base_added <= 0)
    {
        base_added = rogue_item_defs_load_from_cfg("../assets/test_items.cfg");
    }
    assert(base_added > 0);
    int gem_item_def = rogue_item_def_index("legendary_gem");
    assert(gem_item_def >= 0);
    int gems_added = rogue_gem_defs_load_from_cfg("assets/gems_test.cfg");
    if (gems_added <= 0)
    {
        gems_added = rogue_gem_defs_load_from_cfg("../assets/gems_test.cfg");
    }
    assert(gems_added == 2);
    int sword_index = rogue_item_def_index("long_sword");
    assert(sword_index >= 0);
    int inst = rogue_items_spawn(sword_index, 1, 0, 0);
    assert(inst >= 0);
    RogueItemInstance* it_mut = (RogueItemInstance*) rogue_item_instance_at(inst);
    assert(it_mut);
    it_mut->socket_count = 2;
    it_mut->sockets[0] = -1;
    it_mut->sockets[1] = -1;
    rogue_econ_reset();
    rogue_econ_add_gold(1000);
    int gem_flat_index = 0; /* first gem */
    int cost = 0;
    int r = rogue_item_instance_socket_insert_pay(inst, 0, gem_flat_index, &cost);
    assert(r == 0 && cost > 0);
    int gem_pct_index = 1;
    cost = 0;
    r = rogue_item_instance_socket_insert_pay(inst, 1, gem_pct_index, &cost);
    assert(r == 0);
    rogue_equip_equip(ROGUE_EQUIP_WEAPON, inst);
    RoguePlayer player = {0};
    reset_player(&player);
    rogue_equipment_apply_stat_bonuses(&player);
    rogue_stat_cache_force_update(&player);
    assert(g_player_stat_cache.total_strength >= 12);
    assert(g_player_stat_cache.resist_fire >= 3);
    /* Removal refund without returning gem to inventory (avoid inventory subsystem dependency) */
    r = rogue_item_instance_socket_remove_refund(inst, 1, 0);
    assert(r == 0);
    printf("equipment_phase5_gems_ok\n");
    return 0;
}
