/* Tests equipping a two-handed JSON weapon blocks equipping an offhand item (basic rule). */
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>

int main(void)
{
    rogue_equip_reset();
    rogue_item_defs_reset();
    const char* dirs[] = {"assets/items", "../assets/items", "../../assets/items", NULL};
    int loaded = 0;
    for (int i = 0; dirs[i]; i++)
    {
        loaded = rogue_item_defs_load_directory_json(dirs[i]);
        if (loaded > 0)
            break;
    }
    if (loaded <= 0)
    {
        printf("EQUIP_JSON_TWOHAND_FAIL load\n");
        return 10;
    }
    /* Initialize runtime now that definitions are present */
    rogue_items_init_runtime();
    int greatsword = rogue_item_def_index("weapon_greatsword");
    int dagger = rogue_item_def_index("weapon_rare_dagger");
    if (greatsword < 0 || dagger < 0)
    {
        printf("EQUIP_JSON_TWOHAND_FAIL defs\n");
        return 11;
    }
    int inst_great = rogue_items_spawn(greatsword, 1, 0, 0);
    int inst_dagger = rogue_items_spawn(dagger, 1, 0, 0);
    if (inst_great < 0 || inst_dagger < 0)
    {
        printf("EQUIP_JSON_TWOHAND_FAIL spawn\n");
        return 12;
    }
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_great) != 0)
    {
        printf("EQUIP_JSON_TWOHAND_FAIL equip_greatsword\n");
        return 13;
    }
    /* Attempt to equip offhand while two-handed weapon equipped should fail */
    int rc_off = rogue_equip_try(ROGUE_EQUIP_OFFHAND, inst_dagger);
    if (rc_off == 0)
    {
        printf("EQUIP_JSON_TWOHAND_FAIL offhand_allowed rc=%d\n", rc_off);
        return 14;
    }
    /* Unequip weapon then equip dagger in weapon slot should succeed */
    rogue_equip_unequip(ROGUE_EQUIP_WEAPON);
    int rc_weap = rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_dagger);
    if (rc_weap != 0)
    {
        printf("EQUIP_JSON_TWOHAND_FAIL dagger_after_unequip rc=%d\n", rc_weap);
        return 15;
    }
    printf("EQUIP_JSON_TWOHAND_OK offhand_block=%d weap_ok=%d\n", rc_off, rc_weap);
    return 0;
}
