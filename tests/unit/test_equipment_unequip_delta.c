#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/path_utils.h"
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>

/* Verifies stat increase on equip and decrease on unequip (14.5 completion) */
int main()
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("UNEQ_FAIL items\n");
        return 1;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("UNEQ_FAIL load\n");
        return 1;
    }
    rogue_affixes_reset();
    char paff[256];
    if (!rogue_find_asset_path("affixes.cfg", paff, sizeof paff))
    {
        printf("UNEQ_FAIL afffind\n");
        return 2;
    }
    if (rogue_affixes_load_from_cfg(paff) <= 0)
    {
        printf("UNEQ_FAIL affload\n");
        return 2;
    }
    rogue_items_init_runtime();
    int def_index = rogue_item_def_index("long_sword");
    assert(def_index >= 0);
    int inst = rogue_items_spawn(def_index, 1, 0, 0);
    assert(inst >= 0);
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
    assert(it);
    int agility_affix = -1;
    for (int i = 0; i < rogue_affix_count(); i++)
    {
        const RogueAffixDef* a = rogue_affix_at(i);
        if (a && a->stat == ROGUE_AFFIX_STAT_AGILITY_FLAT)
        {
            agility_affix = i;
            break;
        }
    }
    if (agility_affix < 0)
    {
        printf("UNEQ_FAIL no_agility\n");
        return 3;
    }
    it->suffix_index = agility_affix;
    it->suffix_value = 4;
    RoguePlayer p;
    rogue_player_init(&p);
    int base = p.dexterity;
    rogue_equip_reset();
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, inst) != 0)
    {
        printf("UNEQ_FAIL equip\n");
        return 4;
    }
    rogue_equipment_apply_stat_bonuses(&p);
    if (p.dexterity != base + 4)
    {
        printf("UNEQ_FAIL dex_up=%d base=%d\n", p.dexterity, base);
        return 5;
    }
    /* Simulate frame reset to base then unequip */
    p.dexterity = base;
    rogue_equip_unequip(ROGUE_EQUIP_WEAPON);
    rogue_equipment_apply_stat_bonuses(&p);
    if (p.dexterity != base)
    {
        printf("UNEQ_FAIL dex_down=%d base=%d\n", p.dexterity, base);
        return 6;
    }
    printf("UNEQ_OK base=%d eq=%d uneq=%d\n", base, base + 4, p.dexterity);
    return 0;
}
