#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "core/equipment_persist.h"
#include "core/equipment.h"
#include "core/loot_instances.h"

extern int rogue_items_spawn(int def_index,int quantity,float x,float y);

/* Construct a legacy (version 0) equipment block with 3 slots (weapon, head, chest) */
static void build_legacy_v0(char* buf, size_t cap, int w_def, int head_def, int chest_def){
    /* v0 had no header and lines were simply SLOT <idx> DEF <def_index> */
    snprintf(buf,cap,"SLOT 0 DEF %d\nSLOT 1 DEF %d\nSLOT 2 DEF %d\n", w_def, head_def, chest_def);
}

int main(void){
    char legacy[256];
    build_legacy_v0(legacy,sizeof legacy,0,1,2);
    /* Deserialize legacy block */
    int r = rogue_equipment_deserialize(legacy); assert(r==0);
    /* Ensure items were spawned and remapped to modern slots */
    int w = rogue_equip_get(ROGUE_EQUIP_WEAPON); assert(w>=0);
    int head = rogue_equip_get(ROGUE_EQUIP_ARMOR_HEAD); assert(head>=0);
    int chest = rogue_equip_get(ROGUE_EQUIP_ARMOR_CHEST); assert(chest>=0);
    /* Ensure later slots remain empty */
    assert(rogue_equip_get(ROGUE_EQUIP_RING1) < 0);
    /* Round-trip serialize now produces V1 header and SLOT indices in new enum */
    char cur[1024]; int n = rogue_equipment_serialize(cur,(int)sizeof cur); assert(n>0);
    assert(strstr(cur,"EQUIP_V1")!=NULL);
    /* Weapon line should reference modern weapon slot index 0 */
    assert(strstr(cur,"SLOT 0 ")!=NULL);
    printf("equipment_phase13_slot_migration_ok\n");
    return 0;
}
