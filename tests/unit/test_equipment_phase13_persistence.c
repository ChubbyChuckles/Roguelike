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
    /* Expect new tokens present */
    assert(strstr(buf," SET ")!=NULL);
    assert(strstr(buf," RW ")!=NULL);
    unsigned long long h1=rogue_equipment_state_hash(); assert(h1!=0ULL);
    /* Clear equips */
    rogue_equip_unequip(ROGUE_EQUIP_WEAPON); rogue_equip_unequip(ROGUE_EQUIP_RING1);
    /* Deserialize */
    int r=rogue_equipment_deserialize(buf); assert(r==0);
    /* Re-hash */
    unsigned long long h2=rogue_equipment_state_hash(); assert(h1==h2);
    /* Legacy omission simulation: strip SET/UNQ/RW tokens and ensure load still succeeds */
    {
        char legacy[2048]; strncpy(legacy,buf,sizeof legacy); legacy[sizeof legacy-1]='\0';
        char* p=legacy; while((p=strstr(p," SET "))){ char* nl=strchr(p,'\n'); if(!nl) break; /* remove until newline */ memmove(p,nl,strlen(nl)+1); }
        p=legacy; while((p=strstr(p," UNQ "))){ char* nl=strchr(p,'\n'); if(!nl) break; memmove(p,nl,strlen(nl)+1); }
        p=legacy; while((p=strstr(p," RW "))){ char* nl=strchr(p,'\n'); if(!nl) break; memmove(p,nl,strlen(nl)+1); }
        rogue_equip_unequip(ROGUE_EQUIP_WEAPON); rogue_equip_unequip(ROGUE_EQUIP_RING1);
        assert(rogue_equipment_deserialize(legacy)==0);
    }
    /* Tamper test: modify durability number -> hash must differ */
    {
        char tamper[2048]; strncpy(tamper,buf,sizeof tamper); tamper[sizeof tamper-1]='\0';
        char* d=strstr(tamper," DUR "); if(d){ /* find a digit after */ while(*d && *d!='\n' && (*d<'0'||*d>'9')) d++; if(*d>='0'&&*d<='9') *d = (*d=='9')?'1':'9'; }
        rogue_equip_unequip(ROGUE_EQUIP_WEAPON); rogue_equip_unequip(ROGUE_EQUIP_RING1);
        assert(rogue_equipment_deserialize(tamper)==0);
        unsigned long long h3=rogue_equipment_state_hash(); assert(h3!=h1);
    }
    printf("equipment_phase13_persistence_ok\n");
    return 0; }
