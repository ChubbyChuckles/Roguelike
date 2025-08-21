#include "core/equipment/equipment.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

static void reset_all(void) { rogue_equip_reset(); }

int test_equipment_phase1_transmog(void)
{
    reset_all();
    /* Attempt setting transmog with invalid slot */
    assert(rogue_equip_set_transmog(-1, -1) != 0);
    /* Find any armor definition to use as visual override for head slot */
    int armor_def = -1;
    for (int i = 0; i < rogue_item_defs_count(); i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_ARMOR)
        {
            armor_def = i;
            break;
        }
    }
    if (armor_def >= 0)
    {
        int rc = rogue_equip_set_transmog(ROGUE_EQUIP_ARMOR_HEAD, armor_def);
        assert(rc == 0);
        assert(rogue_equip_get_transmog(ROGUE_EQUIP_ARMOR_HEAD) == armor_def);
    }
    /* Ensure category mismatch rejected: use weapon def as head transmog */
    int weapon_def = -1;
    for (int i = 0; i < rogue_item_defs_count(); i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_WEAPON)
        {
            weapon_def = i;
            break;
        }
    }
    if (weapon_def >= 0 && armor_def >= 0)
    {
        int rc_bad = rogue_equip_set_transmog(ROGUE_EQUIP_ARMOR_HEAD, weapon_def);
        assert(rc_bad != 0);
    }
    /* Clear */
    int clr = rogue_equip_set_transmog(ROGUE_EQUIP_ARMOR_HEAD, -1);
    assert(clr == 0);
    assert(rogue_equip_get_transmog(ROGUE_EQUIP_ARMOR_HEAD) == -1);
    return 0;
}

int main(void) { return test_equipment_phase1_transmog(); }
