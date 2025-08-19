#include <stdio.h>
#include <string.h>
#include "entities/enemy.h"
#include "core/app_state.h"
#include "util/log.h"

int main(void){
    RogueEnemyTypeDef types[ROGUE_MAX_ENEMY_TYPES]; int count=0;
    int ok = rogue_enemy_types_load_directory_json("assets/enemies", types, ROGUE_MAX_ENEMY_TYPES, &count);
    if(!ok || count < 2){ printf("FAIL expected >=2 enemy types json loaded count=%d\n", count); return 1; }
    int found_grunt=0, found_elite=0;
    for(int i=0;i<count;i++){
        if(strcmp(types[i].id,"goblin_grunt")==0){ found_grunt=1; if(types[i].group_min!=2 || types[i].group_max!=4){ printf("FAIL grunt group bounds\n"); return 1; } }
        if(strcmp(types[i].id,"goblin_elite")==0){ found_elite=1; if(types[i].base_level_offset!=1 || types[i].tier_id!=2){ printf("FAIL elite lvl/tier mismatch\n"); return 1; } }
    }
    if(!found_grunt||!found_elite){ printf("FAIL missing grunt=%d elite=%d\n", found_grunt, found_elite); return 1; }
    printf("OK enemy_json_loader types=%d\n", count); return 0; }
