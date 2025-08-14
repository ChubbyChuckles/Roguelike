#include "core/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg");
    assert(added>=1);
    const RogueItemDef* gold = rogue_item_def_by_id("gold_coin");
    const RogueItemDef* sword = rogue_item_def_by_id("long_sword");
    const RogueItemDef* staff = rogue_item_def_by_id("magic_staff");
    const RogueItemDef* axe = rogue_item_def_by_id("epic_axe");
    const RogueItemDef* gem = rogue_item_def_by_id("legendary_gem");
    assert(gold && sword && staff && axe && gem);
    assert(gold->rarity == 0);
    assert(sword->rarity == 1);
    assert(staff->rarity == 2);
    assert(axe->rarity == 3);
    assert(gem->rarity == 4);
    printf("LOOT_RARITY_PARSE_OK rarities=%d,%d,%d,%d,%d\n", gold->rarity,sword->rarity,staff->rarity,axe->rarity,gem->rarity);
    return 0;
}
