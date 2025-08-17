/* Phase 20.5 Stress Test: Max ground items + merge behavior
 * - Spawns many identical stackable items clustered to trigger merges.
 * - Verifies active count well below spawn attempts (merges occurred) and never exceeds cap.
 * - Ensures no crash and merge ratio reasonable (>20% merges).
 */
#include <stdio.h>
#include <assert.h>
#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include "core/loot_instances.h"
#include "core/path_utils.h"
#include "core/app_state.h"

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    rogue_item_defs_reset(); char pitems[256]; assert(rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)); assert(rogue_item_defs_load_from_cfg(pitems)>0);
    rogue_loot_tables_reset(); char ptables[256]; assert(rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables)); assert(rogue_loot_tables_load_from_cfg(ptables)>0);

    rogue_items_init_runtime();
    int gold_index = rogue_item_def_index("gold_coin"); assert(gold_index>=0);

    int attempts = 200; int spawned=0;
    for(int i=0;i<attempts;i++){
        float ox = 5.0f + (float)(i%3)*0.2f; float oy = 5.0f + (float)((i/3)%3)*0.2f; /* small grid ensures adjacency within merge radius */
        int inst = rogue_items_spawn(gold_index, 1, ox, oy); if(inst>=0) spawned++;
    }
    int active = rogue_items_active_count();
    if(active > ROGUE_ITEM_INSTANCE_CAP){ fprintf(stderr,"FAIL: active exceeds cap %d>%d\n", active, ROGUE_ITEM_INSTANCE_CAP); return 5; }
    int merges = spawned - active; if(merges <= 0){ fprintf(stderr,"FAIL: no merges occurred spawned=%d active=%d\n", spawned, active); return 6; }
    float merge_ratio = (float)merges / (float)spawned;
    if(merge_ratio < 0.20f){ fprintf(stderr,"FAIL: insufficient merge ratio %.2f\n", merge_ratio); return 7; }
    printf("loot_merge_stress_ok spawned=%d active=%d merges=%d ratio=%.2f\n", spawned, active, merges, merge_ratio);
    return 0;
}
