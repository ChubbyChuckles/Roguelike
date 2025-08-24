#include "../../src/core/equipment/equipment.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/vendor/economy.h"
#include "../../src/util/path_utils.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("REPAIR_FAIL items\n");
        return 1;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("REPAIR_FAIL load\n");
        return 1;
    }
    rogue_items_init_runtime();
    int def_index = rogue_item_def_index("long_sword");
    assert(def_index >= 0);
    int inst = rogue_items_spawn(def_index, 1, 0, 0);
    assert(inst >= 0);
    const RogueItemInstance* itc = rogue_item_instance_at(inst);
    assert(itc);
    int max = itc->durability_max;
    if (max <= 10)
    {
        printf("REPAIR_FAIL max=%d\n", max);
        return 2;
    }
    /* Damage durability by 10 */
    for (int i = 0; i < 10; i++)
        rogue_item_instance_damage_durability(inst, 1);
    const RogueItemInstance* itd = rogue_item_instance_at(inst);
    int missing = max - itd->durability_cur;
    if (missing != 10)
    {
        printf("REPAIR_FAIL missing=%d\n", missing);
        return 3;
    }
    rogue_econ_reset();
    rogue_econ_add_gold(10000);
    rogue_equip_reset();
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, inst) != 0)
    {
        printf("REPAIR_FAIL equip\n");
        return 4;
    }
    int expected = rogue_econ_repair_cost(missing, rogue_item_def_at(itd->def_index)->rarity);
    int gold_before = rogue_econ_gold();
    int rc = rogue_equip_repair_slot(ROGUE_EQUIP_WEAPON);
    if (rc != 0)
    {
        printf("REPAIR_FAIL repair rc=%d\n", rc);
        return 5;
    }
    int gold_after = rogue_econ_gold();
    const RogueItemInstance* itr = rogue_item_instance_at(inst);
    if (itr->durability_cur != itr->durability_max)
    {
        printf("REPAIR_FAIL not_full %d/%d\n", itr->durability_cur, itr->durability_max);
        return 6;
    }
    if (gold_before - gold_after != expected)
    {
        printf("REPAIR_FAIL gold delta=%d expected=%d\n", gold_before - gold_after, expected);
        return 7;
    }
    printf("REPAIR_OK cost=%d missing=%d max=%d\n", expected, missing, max);
    return 0;
}
