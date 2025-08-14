#include "core/loot_item_defs.h"
#include "core/loot_instances.h"
#include "core/app_state.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg");
    assert(added >= 3);
    rogue_items_init_runtime();
    int coin = rogue_item_def_index("gold_coin"); assert(coin>=0);
    int a = rogue_items_spawn(coin,5, 10.0f,10.0f); assert(a>=0);
    int b = rogue_items_spawn(coin,7, 10.2f,10.1f); assert(b>=0);
    rogue_items_update(16.0f);
    int active=0; int total_qty=0; for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active){ active++; total_qty += g_app.item_instances[i].quantity; }
    assert(total_qty == 12);
    assert(active <= 2);
    rogue_items_update((float)ROGUE_ITEM_DESPAWN_MS + 1.0f);
    int remain=0; for(int i=0;i<g_app.item_instance_cap;i++) if(g_app.item_instances[i].active) remain++;
    assert(remain==0);
    printf("LOOT_MERGE_DESPAWN_OK merged=%d total=%d remain=%d\n", active,total_qty,remain);
    return 0;
}
