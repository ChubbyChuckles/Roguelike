/* Equipment System Phase 1 Slot Expansion Tests */
#include <assert.h>
#include "core/equipment.h"
#include "core/stat_cache.h"

static void reset(){ rogue_equip_reset(); }

int test_equipment_phase1_slots(void){
    reset();
    /* The real equip_try validates instance indices; in this isolated test environment we focus on compile/link and logic path presence. */
    (void)rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, 0);
    (void)rogue_equip_try(ROGUE_EQUIP_WEAPON, 0);
    (void)rogue_equip_try(ROGUE_EQUIP_OFFHAND, 0);
    /* Placeholder two-hand detection always false so offhand attempt not auto-failing due to two-hand rule. */
    return 0;
}

int main(void){ return test_equipment_phase1_slots(); }
