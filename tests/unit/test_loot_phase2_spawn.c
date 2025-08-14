#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include "core/loot_instances.h"
#include "core/app_state.h"
#include <assert.h>
#include <stdio.h>

/* Minimal stub state for loot spawn logic unit test (without full enemy system). */
RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg("../assets/test_items.cfg"); assert(added>=3);
    rogue_loot_tables_reset();
    int tadded = rogue_loot_tables_load_from_cfg("../assets/test_loot_tables.cfg"); assert(tadded>=1);
    rogue_items_init_runtime();
    unsigned int seed=123u;
    int idx = rogue_loot_table_index("GOBLIN_BASIC"); assert(idx>=0);
    int idef[8]; int qty[8]; int drops = rogue_loot_roll(idx,&seed,8,idef,qty); assert(drops>=1);
    int before = rogue_items_active_count();
    for(int i=0;i<drops;i++){ if(idef[i]>=0) rogue_items_spawn(idef[i], qty[i], 5.0f, 6.0f); }
    int after = rogue_items_active_count();
    assert(after >= before + drops - 1); /* allow duplicates filtering by no-empty slots */
    printf("LOOT_SPAWN_OK count=%d\n", after);
    return 0;
}
