/* Phase 20.4 Save/Load Round-Trip Stress (affix-rich items)
 * - Generates a batch of items via generation pipeline across multiple loot tables with varying context seeds.
 * - Captures snapshot of active item instances (subset of fields) before and after full save/load.
 * - Asserts 100% field equality for: def_index, quantity, rarity, prefix/suffix index+value, durability_cur/max, enchant_level.
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include "core/loot_affixes.h"
#include "core/loot_generation.h"
#include "core/loot_instances.h"
#include "core/loot_rarity_adv.h"
#include "core/save_manager.h"
#include "core/path_utils.h"
#include "core/app_state.h"

/* Required globals for core systems used in persistence */
RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

typedef struct SnapRec { int def_index, qty, rarity, pidx,pval,sidx,sval, dcur,dmax, enchant; } SnapRec;

static int snapshot(SnapRec* out, int cap){
    int count=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP && count<cap;i++){ const RogueItemInstance* it = rogue_item_instance_at(i); if(!it||!it->active) continue; out[count++] = (SnapRec){ it->def_index,it->quantity,it->rarity,it->prefix_index,it->prefix_value,it->suffix_index,it->suffix_value,it->durability_cur,it->durability_max,it->enchant_level }; }
    return count;
}

int main(void){
    /* Load prerequisite content */
    rogue_item_defs_reset(); char pitems[256]; if(!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)){ fprintf(stderr,"FAIL: find test_items.cfg\n"); return 1; } if(rogue_item_defs_load_from_cfg(pitems)<=0){ fprintf(stderr,"FAIL: load test_items.cfg\n"); return 1; }
    rogue_loot_tables_reset(); char ptables[256]; if(!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables)){ fprintf(stderr,"FAIL: find test_loot_tables.cfg\n"); return 1; } if(rogue_loot_tables_load_from_cfg(ptables)<=0){ fprintf(stderr,"FAIL: load test_loot_tables.cfg\n"); return 1; }
    rogue_affixes_reset(); char paff[256]; if(!rogue_find_asset_path("affixes.cfg", paff, sizeof paff)){ fprintf(stderr,"FAIL: find affixes.cfg\n"); return 1; } if(rogue_affixes_load_from_cfg(paff)<0){ fprintf(stderr,"FAIL: load affixes.cfg\n"); return 1; }
    rogue_rarity_adv_reset();
    rogue_items_init_runtime(); /* ensure spawn arrays ready prior to generation */

    /* Register save components (inventory needed) */
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init(); /* ensure migrations + init flags set (mirrors other save tests) */
    rogue_register_core_save_components();

    /* Early diagnostic: verify all components registered before any generation */
    int early_rc = rogue_save_manager_save_slot(0);
    if(early_rc==0){ char ej[256]; if(rogue_save_export_json(0,ej,sizeof ej)==0){ fprintf(stderr,"DIAG: early save JSON=%s\n", ej); } }

    /* Generate items across tables */
    int tbl_orc = rogue_loot_table_index("ORC_BASE");
    int tbl_skel = rogue_loot_table_index("SKELETON_BASE");
    if(tbl_orc<0||tbl_skel<0){ fprintf(stderr,"FAIL: loot table indices orc=%d skel=%d\n", tbl_orc,tbl_skel); return 3; }
    RogueGenerationContext ctx = {0};
    unsigned int seed = 0x1234ABCDu;
    int target_items = 90; /* well below cap but large enough for variety */
    for(int i=0;i<target_items;i++){
        ctx.enemy_level = (i%40); ctx.player_luck = (i%15); ctx.biome_id = i%5; ctx.enemy_archetype = i%7;
        int tbl = (i&1)? tbl_orc: tbl_skel;
        RogueGeneratedItem gi; if(rogue_generate_item(tbl,&ctx,&seed,&gi)==0){
            /* Randomly damage durability if present (simulate gameplay changes) */
            if(gi.inst_index>=0){ int cur,max; if(rogue_item_instance_get_durability(gi.inst_index,&cur,&max)==0 && max>0){ rogue_item_instance_damage_durability(gi.inst_index, (i%3)==0?1:0); }}
        }
    }
    int active_gen=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ const RogueItemInstance* it=rogue_item_instance_at(i); if(it&&it->active) active_gen++; }
    if(active_gen<=0){ fprintf(stderr,"FAIL: no active items after generation\n"); return 4; }
    SnapRec before[ROGUE_ITEM_INSTANCE_CAP]; int bcount = snapshot(before, ROGUE_ITEM_INSTANCE_CAP); if(bcount<=0){ fprintf(stderr,"FAIL: no items generated bcount=%d\n", bcount); return 2; }

    /* Re-register components defensively (diagnostic) */
    rogue_register_core_save_components();

    /* Full save slot 0 */
    int save_rc = rogue_save_manager_save_slot(0);
    if(save_rc!=0){ fprintf(stderr,"FAIL: save rc=%d\n", save_rc); return 5; }
    /* Diagnostic: export JSON summary */
    char json[512]; int jrc=rogue_save_export_json(0,json,sizeof json); if(jrc==0){ fprintf(stderr,"DIAG: save JSON=%s\n", json); } else { fprintf(stderr,"DIAG: save JSON export failed rc=%d\n", jrc); }
    int inv_only_rc = rogue_save_manager_save_slot_inventory_only(0); fprintf(stderr,"DIAG: inventory_only_save rc=%d (expect 0)\n", inv_only_rc);
    /* Clear runtime and reload */
    /* For isolation: reset item defs & reload, clear instances via runtime reset (simple way: shutdown+init not exposed here; rely on loading to overwrite) */
    rogue_items_init_runtime(); /* ensure runtime arrays valid if not already */
    /* Simulate app restart by resetting item defs / tables / affixes then reloading from save */
    rogue_item_defs_reset(); assert(rogue_item_defs_load_from_cfg(pitems)>0);
    rogue_loot_tables_reset(); assert(rogue_loot_tables_load_from_cfg(ptables)>0);
    rogue_affixes_reset(); assert(rogue_affixes_load_from_cfg(paff)>=0);
    /* Load slot */
        /* Load slot (don't rely on assert side effects; ensure executed under Release) */
        int load_rc = rogue_save_manager_load_slot(0);
        if(load_rc!=0){ fprintf(stderr,"FAIL: load rc=%d\n", load_rc); return 6; }

    /* Diagnostic: dump first few raw instances after load */
    int raw_active=0; for(int i=0;i<ROGUE_ITEM_INSTANCE_CAP;i++){ const RogueItemInstance* it=rogue_item_instance_at(i); if(it) raw_active++; }
    fprintf(stderr,"DIAG: raw_active_after_load=%d\n", raw_active);
    for(int i=0;i<10;i++){ const RogueItemInstance* it=rogue_item_instance_at(i); if(it){ fprintf(stderr,"  RAW[%d]: def=%d qty=%d rar=%d pidx=%d sidx=%d dur=%d/%d ench=%d active=1\n", i,it->def_index,it->quantity,it->rarity,it->prefix_index,it->suffix_index,it->durability_cur,it->durability_max,it->enchant_level); } else { fprintf(stderr,"  RAW[%d]: <inactive>\n", i); } }

    SnapRec after[ROGUE_ITEM_INSTANCE_CAP]; int acount = snapshot(after, ROGUE_ITEM_INSTANCE_CAP); assert(acount==bcount);

    /* Compare order-insensitive: build simple multiset match counts */
    int matched=0; int used[ROGUE_ITEM_INSTANCE_CAP]; memset(used,0,sizeof used);
    for(int i=0;i<bcount;i++){
        for(int j=0;j<acount;j++) if(!used[j]){
            if(memcmp(&before[i], &after[j], sizeof(SnapRec))==0){ used[j]=1; matched++; break; }
        }
    }
    if(matched != bcount){
        fprintf(stderr,"FAIL: persistence roundtrip mismatch matched=%d/%d\n", matched, bcount);
        int dump = bcount<10? bcount:10;
        fprintf(stderr,"Before records (first %d):\n", dump);
        for(int i=0;i<dump;i++){
            SnapRec* r=&before[i];
            fprintf(stderr,"  B[%d]: def=%d qty=%d rar=%d p(%d,%d) s(%d,%d) dur=%d/%d ench=%d\n", i,r->def_index,r->qty,r->rarity,r->pidx,r->pval,r->sidx,r->sval,r->dcur,r->dmax,r->enchant);
        }
        fprintf(stderr,"After records (first %d):\n", dump);
        for(int i=0;i<dump;i++){
            SnapRec* r=&after[i];
            fprintf(stderr,"  A[%d]: def=%d qty=%d rar=%d p(%d,%d) s(%d,%d) dur=%d/%d ench=%d\n", i,r->def_index,r->qty,r->rarity,r->pidx,r->pval,r->sidx,r->sval,r->dcur,r->dmax,r->enchant);
        }
        /* Find first before record and show closest after record (field-wise) */
        if(bcount>0 && acount>0){
            int bi=0; int best_j=-1; int best_diff=100;
            for(int j=0;j<acount;j++){
                int diff=0; const int* b=(const int*)&before[bi]; const int* a=(const int*)&after[j];
                for(size_t k=0;k<sizeof(SnapRec)/sizeof(int);k++) if(b[k]!=a[k]) diff++;
                if(diff<best_diff){ best_diff=diff; best_j=j; }
            }
            if(best_j>=0){
                const int* b=(const int*)&before[0]; const int* a=(const int*)&after[best_j];
                fprintf(stderr,"Field diffs for first before vs after[%d]:\n", best_j);
                const char* names[] = {"def","qty","rar","pidx","pval","sidx","sval","dcur","dmax","ench"};
                for(size_t k=0;k<sizeof(SnapRec)/sizeof(int);k++) if(b[k]!=a[k]) fprintf(stderr,"  %s: %d -> %d\n", names[k], b[k], a[k]);
            }
        }
        return 10;
    }
    printf("loot_persistence_roundtrip_ok count=%d\n", bcount);
    return 0;
}
