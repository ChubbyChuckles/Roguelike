#include "../../src/core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg");
    assert(added >= 1);
    const RogueItemDef* gold = rogue_item_def_by_id("gold_coin");
    assert(gold);
    assert(gold->rarity == 0);
    printf("LOOT_RARITY_PARSE_OK rarity=%d\n", gold->rarity);
    return 0;
}
