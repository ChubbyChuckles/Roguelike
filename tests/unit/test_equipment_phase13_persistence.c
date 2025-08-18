#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "core/equipment_persist.h"
#include "core/equipment.h"
#include "core/loot_instances.h"

/* Basic setup helpers (spawn simple dummy items). We assume item defs 0..2 exist in test assets. */
extern int rogue_items_spawn(int def_index,int quantity,float x,float y);

int main(void){
    /* Spawn and equip two items */
    int w=rogue_items_spawn(0,1,0,0); assert(w>=0); rogue_equip_try(ROGUE_EQUIP_WEAPON,w);
    int ring=rogue_items_spawn(1,1,0,0); assert(ring>=0); rogue_equip_try(ROGUE_EQUIP_RING1,ring);
    /* Serialize */
    char buf[2048]; int n=rogue_equipment_serialize(buf,(int)sizeof buf); assert(n>0); assert(strstr(buf,"EQUIP_V1"));
    unsigned long long h1=rogue_equipment_state_hash(); assert(h1!=0ULL);
    /* Clear equips */
    rogue_equip_unequip(ROGUE_EQUIP_WEAPON); rogue_equip_unequip(ROGUE_EQUIP_RING1);
    /* Deserialize */
    int r=rogue_equipment_deserialize(buf); assert(r==0);
    /* Re-hash */
    unsigned long long h2=rogue_equipment_state_hash(); assert(h1==h2);
    printf("equipment_phase13_persistence_ok\n");
    return 0; }
