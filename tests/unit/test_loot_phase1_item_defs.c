#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/util/path_utils.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    int ok = rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems);
    assert(ok);
    int added = rogue_item_defs_load_from_cfg(pitems);
    assert(added >= 3);
    const RogueItemDef* gold = rogue_item_def_by_id("gold_coin");
    assert(gold && gold->stack_max > 1);
    const RogueItemDef* sword = rogue_item_def_by_id("long_sword");
    assert(sword && sword->base_damage_max >= sword->base_damage_min);
    printf("LOOT_ITEM_DEFS_OK\n");
    return 0;
}
