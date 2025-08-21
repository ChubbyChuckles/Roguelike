#include "core/crafting/crafting.h"
#include "core/loot/loot_item_defs.h"
#include "core\inventory\inventory.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <string.h>

static int inv_get(int di){ return rogue_inventory_get_count(di); }
static int inv_add(int di,int q){ return rogue_inventory_add(di,q); }
static int inv_consume(int di,int q){ return rogue_inventory_consume(di,q); }

int main(void){
    char pitems[256]; if(!rogue_find_asset_path("items/materials.cfg", pitems, sizeof pitems)){ fprintf(stderr,"CRAFT_FAIL find materials\n"); return 10; }
    for(int i=(int)strlen(pitems)-1;i>=0;i--){ if(pitems[i]=='/'||pitems[i]=='\\'){ pitems[i]='\0'; break; } }
    rogue_item_defs_reset(); if(rogue_item_defs_load_directory(pitems)<=0){ fprintf(stderr,"CRAFT_FAIL load dir\n"); return 11; }
    rogue_inventory_reset();
    int dust = rogue_item_def_index("arcane_dust"); int shard = rogue_item_def_index("primal_shard"); if(dust<0||shard<0){ fprintf(stderr,"CRAFT_FAIL mat defs\n"); return 12; }
    if(rogue_material_tier(dust) != 1){ fprintf(stderr,"CRAFT_FAIL dust tier\n"); return 13; }
    if(rogue_material_tier(shard) != 3){ fprintf(stderr,"CRAFT_FAIL shard tier\n"); return 14; }
    /* Fake recipe: create primal_shard from arcane_dust x5 */
    const char* recipe_path = "tmp_recipe.cfg";
    FILE* f = fopen(recipe_path, "wb"); if(!f){ fprintf(stderr,"CRAFT_FAIL tmp open\n"); return 15; }
    fprintf(f, "dust_to_shard,primal_shard,1,arcane_dust:5,\n"); fclose(f);
    if(rogue_craft_load_file(recipe_path) <= 0){ fprintf(stderr,"CRAFT_FAIL load recipe\n"); return 16; }
    const RogueCraftRecipe* r = rogue_craft_find("dust_to_shard"); if(!r){ fprintf(stderr,"CRAFT_FAIL find recipe\n"); return 17; }
    rogue_inventory_add(dust,5);
    if(rogue_craft_execute(r, inv_get, inv_consume, inv_add) != 0){ fprintf(stderr,"CRAFT_FAIL exec\n"); return 18; }
    if(rogue_inventory_get_count(shard) < 1){ fprintf(stderr,"CRAFT_FAIL no shard\n"); return 19; }
    printf("CRAFT_OK recipes=%d shard=%d\n", rogue_craft_recipe_count(), rogue_inventory_get_count(shard));
    return 0;
}
