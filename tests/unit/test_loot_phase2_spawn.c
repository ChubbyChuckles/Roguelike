#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include "core/loot_instances.h"
#include "core/path_utils.h"
#include "core/app_state.h"
#include <assert.h>
#include <stdio.h>

/* Minimal stub state for loot spawn logic unit test (without full enemy system). */
RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    rogue_item_defs_reset();
    char pitems[256]; assert(rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems));
    int added = rogue_item_defs_load_from_cfg(pitems); assert(added>=3);
    rogue_loot_tables_reset();
    char ptables[256]; assert(rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables));
    int tadded = rogue_loot_tables_load_from_cfg(ptables); if(tadded<1){ fprintf(stderr,"LOAD_TABLES_FAIL added=%d path=%s\n", tadded, ptables); return 1; }
    rogue_items_init_runtime();
    unsigned int seed=123u;
    int idx = rogue_loot_table_index("ORC_BASE"); assert(idx>=0);
    int idef[8]; int qty[8]; int drops = rogue_loot_roll(idx,&seed,8,idef,qty); if(drops<1){ fprintf(stderr,"LOOT_SPAWN_FAIL drops=%d idx=%d entries=%d tables=%d path=%s\n", drops, idx, rogue_item_defs_count(), rogue_loot_tables_count(), ptables); return 3; }
    int before = rogue_items_active_count();
    for(int i=0;i<drops;i++){ if(idef[i]>=0) rogue_items_spawn(idef[i], qty[i], 5.0f, 6.0f); }
    int after = rogue_items_active_count();
    if(!(after >= before + drops - 1)){ fprintf(stderr,"LOOT_SPAWN_COUNT_FAIL before=%d after=%d drops=%d\n", before, after, drops); return 4; }
    printf("LOOT_SPAWN_OK count=%d\n", after);
    return 0;
}
