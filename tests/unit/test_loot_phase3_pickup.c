#include "../../src/core/app_state.h"
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_pickup.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/path_utils.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    assert(rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems));
    int added = rogue_item_defs_load_from_cfg(pitems);
    assert(added >= 3);
    rogue_loot_tables_reset();
    char ptables[256];
    assert(rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables));
    int tadded = rogue_loot_tables_load_from_cfg(ptables);
    assert(tadded >= 1);
    rogue_items_init_runtime();
    rogue_inventory_init();
    g_app.player.base.pos.x = 5.0f;
    g_app.player.base.pos.y = 5.0f;
    int coin_index = rogue_item_def_index("gold_coin");
    assert(coin_index >= 0);
    int inst = rogue_items_spawn(coin_index, 3, 5.2f, 5.1f);
    assert(inst >= 0);
    assert(rogue_inventory_get_count(coin_index) == 0);
    rogue_loot_pickup_update(0.6f);
    assert(rogue_inventory_get_count(coin_index) == 3);
    assert(rogue_items_active_count() == 0);
    printf("LOOT_PICKUP_OK count=%d\n", rogue_inventory_get_count(coin_index));
    return 0;
}
