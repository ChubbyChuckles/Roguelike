/* Phase 6.3: Transmute (enchant/reforge) material scaling integration test */
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_affixes.h"
#include "core/equipment/equipment_enchant.h"
#include "core/crafting/material_registry.h"
#include "core/path_utils.h"
#include <stdio.h>

/* For this focused cost-scaling test we don't assert material consumption, so allow bypass */
#define ROGUE_TEST_DISABLE_CATALYSTS 1

/* Stubs */
int rogue_inventory_get_count(int def_index){ (void)def_index; return 50; }
int rogue_inventory_consume(int def_index,int qty){ (void)def_index; (void)qty; return qty; }
int rogue_econ_gold(void){ return 1000000; }
int rogue_econ_add_gold(int delta){ (void)delta; return 0; }
void rogue_stat_cache_mark_dirty(void){}

static int load_items_affixes_materials(void){
    rogue_item_defs_reset();
    const char* dirs[] = {"assets/items","../assets/items","../../assets/items","../../../assets/items"};
    int loaded=0; for(size_t i=0;i<sizeof(dirs)/sizeof(dirs[0]); ++i){ int r=rogue_item_defs_load_directory(dirs[i]); if(r>0) loaded+=r; }
    /* Also load generic test items to ensure presence of enchant_orb/reforge_hammer catalysts */
    int extra = rogue_item_defs_load_from_cfg("assets/test_items.cfg"); if(extra<=0) extra = rogue_item_defs_load_from_cfg("../assets/test_items.cfg"); if(extra<=0) extra = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg"); if(extra<=0) extra = rogue_item_defs_load_from_cfg("../../../assets/test_items.cfg"); if(extra>0) loaded += extra;
    if(loaded<=0){ fprintf(stderr,"P6_3_LOAD fail item defs (dirs tried=%zu)\n", sizeof(dirs)/sizeof(dirs[0])); return 0; }
    rogue_affixes_reset(); char paff[256]; if(!rogue_find_asset_path("affixes.cfg", paff, sizeof paff)){ fprintf(stderr,"P6_3_LOAD fail affix_path\n"); return 0; } if(rogue_affixes_load_from_cfg(paff)<=0){ fprintf(stderr,"P6_3_LOAD fail affixes\n"); return 0; }
    /* load materials for tier scaling */
    if(rogue_material_registry_load_default()<=0){ fprintf(stderr,"P6_3_LOAD fail materials\n"); return 0; }
    return 1;
}

int main(void){
    if(!load_items_affixes_materials()){ fprintf(stderr,"CRAFT_P6_3_FAIL load_data\n"); return 10; }
    rogue_items_init_runtime();
    /* Spawn a rare weapon (rarity>=3) to guarantee both affixes after reforge */
    int target=-1; for(int i=0;i<2048;i++){ const RogueItemDef* d=rogue_item_def_at(i); if(!d) break; if(d->category==ROGUE_ITEM_WEAPON && d->rarity>=3){ target=i; break; } }
    if(target<0){ fprintf(stderr,"CRAFT_P6_3_FAIL no_target\n"); return 11; }
    int inst = rogue_items_spawn(target,1,0,0); if(inst<0){ fprintf(stderr,"CRAFT_P6_3_FAIL spawn\n"); return 12; }
    /* Ensure we have at least one prefix/suffix by applying manual generation if missing */
    unsigned int seed=1337u; if(rogue_item_instance_generate_affixes(inst,&seed,3)!=0){ fprintf(stderr,"CRAFT_P6_3_FAIL gen_affix\n"); return 13; }
    int cost1=0; /* Only reroll prefix to avoid needing enchant material for both */
    if(rogue_item_instance_enchant(inst,1,0,&cost1)!=0){ fprintf(stderr,"CRAFT_P6_3_FAIL enchant1\n"); return 14; }
    int cost2=0; if(rogue_item_instance_reforge(inst,&cost2)!=0){ fprintf(stderr,"CRAFT_P6_3_FAIL reforge\n"); return 15; }
    if(cost2 <= cost1){ fprintf(stderr,"CRAFT_P6_3_FAIL cost_relation c1=%d c2=%d\n", cost1, cost2); return 16; }
    printf("CRAFT_P6_3_OK cost_enchant=%d cost_reforge=%d\n", cost1,cost2);
    return 0;
}
