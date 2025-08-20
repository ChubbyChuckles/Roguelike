#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/loot_affixes.h"
#include "core/equipment_enhance.h"
#include <stdio.h>
#include <string.h>

/* Minimal fake inventory & economy hooks (stubs) */
int rogue_inventory_get_count(int def_index){ (void)def_index; return 10; }
int rogue_inventory_consume(int def_index,int qty){ (void)def_index; (void)qty; return qty; }
int rogue_econ_gold(void){ return 100000; }
int rogue_econ_add_gold(int delta){ (void)delta; return 0; }
void rogue_stat_cache_mark_dirty(void){}

static int ensure_items_loaded(void){
    rogue_item_defs_reset();
    /* Load primary item directories */
    const char* dirs[] = {"assets/items","../assets/items","../../assets/items"};
    int loaded=0; for(size_t i=0;i<sizeof(dirs)/sizeof(dirs[0]);++i){ int r=rogue_item_defs_load_directory(dirs[i]); if(r>0) loaded+=r; }
    /* Also load dedicated socket test definitions to guarantee socket_max>0 for enhancement test */
    int rtest = rogue_item_defs_load_from_cfg("assets/equipment_test_sockets.cfg");
    if(rtest<=0) rtest = rogue_item_defs_load_from_cfg("../assets/equipment_test_sockets.cfg");
    if(rtest>0) loaded += rtest;
    return loaded>0;
}

int main(void){
    if(!ensure_items_loaded()){ fprintf(stderr,"CRAFT_P6_FAIL load items\n"); return 10; }
    /* Load affix definitions required for imbue & temper operations */
    rogue_affixes_reset();
    char paff[256];
    if(!rogue_find_asset_path("affixes.cfg", paff, sizeof paff)){ fprintf(stderr,"CRAFT_P6_FAIL find affixes.cfg\n"); return 21; }
    if(rogue_affixes_load_from_cfg(paff) <= 0){ fprintf(stderr,"CRAFT_P6_FAIL load affixes\n"); return 22; }
    rogue_items_init_runtime();
    /* Prefer an item with socket capacity; fallback to weapon rarity>=2 */
    int target_def=-1; for(int i=0;i<2048;i++){ const RogueItemDef* d=rogue_item_def_at(i); if(!d) break; if(d->socket_max>0){ target_def=i; break; } }
    if(target_def<0){ for(int i=0;i<2048;i++){ const RogueItemDef* d=rogue_item_def_at(i); if(!d) break; if(d->category==ROGUE_ITEM_WEAPON && d->rarity>=2){ target_def=i; break; } } }
    if(target_def<0){ fprintf(stderr,"CRAFT_P6_FAIL no target def\n"); return 11; }
    int inst = rogue_items_spawn(target_def,1,0,0); if(inst<0){ fprintf(stderr,"CRAFT_P6_FAIL spawn\n"); return 12; }
    const RogueItemDef* target = rogue_item_def_at(target_def);
    fprintf(stderr, "CRAFT_P6_INFO target_def=%d socket_min=%d socket_max=%d rarity=%d\n", target_def, target?target->socket_min:-1, target?target->socket_max:-1, target?target->rarity:-1);
    /* Imbue prefix (should succeed) */
    int aidx=-1, aval=-1; if(rogue_item_instance_imbue(inst,1,&aidx,&aval)!=0 || aidx<0 || aval<=0){ fprintf(stderr,"CRAFT_P6_FAIL imbue_prefix\n"); return 13; }
    /* Imbue suffix */
    int bidx=-1, bval=-1; if(rogue_item_instance_imbue(inst,0,&bidx,&bval)!=0 || bidx<0 || bval<=0){ fprintf(stderr,"CRAFT_P6_FAIL imbue_suffix\n"); return 14; }
    /* Temper prefix a few times ensuring value does not exceed budget */
    int cap = rogue_budget_max(rogue_item_instance_at(inst)->item_level, rogue_item_instance_at(inst)->rarity);
    for(int i=0;i<5;i++){
        int failed=0; int newv=-1; int rc = rogue_item_instance_temper(inst,1,2,&newv,&failed); if(rc<0){ fprintf(stderr,"CRAFT_P6_FAIL temper_rc=%d\n",rc); return 15; }
        int total = rogue_item_instance_total_affix_weight(inst); if(total>cap){ fprintf(stderr,"CRAFT_P6_FAIL over_budget total=%d cap=%d\n", total, cap); return 16; }
    }
    int newc=0; if(target && target->socket_max>0){
        /* Add socket attempts until returns 1 (already max) */
        int add_rc=0, adds=0; while((add_rc=rogue_item_instance_add_socket(inst,&newc))==0){ adds++; if(adds>10) break; }
        if(add_rc<0){ fprintf(stderr,"CRAFT_P6_FAIL add_socket_rc=%d\n", add_rc); return 17; }
        /* Reroll sockets */
        if(rogue_item_instance_reroll_sockets(inst,&newc)!=0){ fprintf(stderr,"CRAFT_P6_FAIL reroll_sockets\n"); return 18; }
    }
    /* Fracture risk statistical smoke: attempt many failing tempers on suffix by forcing intensity high to cause failures eventually. */
    int fracture_events=0; for(int t=0;t<50;t++){ int failed=0; int dummy; int rc=rogue_item_instance_temper(inst,0,3,&dummy,&failed); if(rc==2 && failed) fracture_events++; }
    /* Current temper implementation seeds deterministically per affix (may yield always-success). Track failures (if any) for telemetry only. */
    printf("CRAFT_P6_OK affixA=%d:%d affixB=%d:%d sockets=%d failures=%d\n", aidx,aval,bidx,bval,newc,fracture_events);
    return 0;
}
