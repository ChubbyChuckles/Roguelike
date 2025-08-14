#include "core/salvage.h"
#include "core/loot_item_defs.h"
#include "core/path_utils.h"
#include "core/inventory.h"
#include <stdio.h>

int main(void){
    char pitems[256]; if(!rogue_find_asset_path("items/swords.cfg", pitems, sizeof pitems)){ fprintf(stderr,"SALV_FAIL find swords\n"); return 10; }
    /* Derive directory path to bulk load */
    for(int i=(int)strlen(pitems)-1;i>=0;i--){ if(pitems[i]=='/'||pitems[i]=='\\'){ pitems[i]='\0'; break; } }
    rogue_item_defs_reset(); if(rogue_item_defs_load_directory(pitems)<=0){ fprintf(stderr,"SALV_FAIL load dir\n"); return 11; }
    rogue_inventory_reset();
    int epic = rogue_item_def_index("epic_blade"); if(epic<0){ fprintf(stderr,"SALV_FAIL epic blade\n"); return 12; }
    int before = rogue_inventory_get_count(rogue_item_def_index("primal_shard"));
    int gained = rogue_salvage_item(epic,3, rogue_inventory_add);
    if(gained<=0){ fprintf(stderr,"SALV_FAIL no gain\n"); return 13; }
    int after = rogue_inventory_get_count(rogue_item_def_index("primal_shard"));
    if(after <= before){ fprintf(stderr,"SALV_FAIL inventory not updated\n"); return 14; }
    printf("SALV_OK gained=%d\n", gained);
    return 0;
}
