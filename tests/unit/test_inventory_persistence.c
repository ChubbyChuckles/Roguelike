#include "core\inventory\inventory.h"
#include "core/persistence.h"
#include "core/app_state.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app; RoguePlayer g_exposed_player_for_stats; void rogue_player_recalc_derived(RoguePlayer* p){ (void)p; }

int main(void){
    rogue_persistence_set_paths("test_inv_player_stats.cfg","test_inv_gen_params.cfg");
    rogue_inventory_init();
    g_app.player.level=1; g_app.player.xp=0; g_app.player.xp_to_next=10;
    rogue_inventory_add(2,7);
    rogue_inventory_add(5,11);
    rogue_persistence_save_player_stats();
    /* reset and reload */
    rogue_inventory_reset();
    assert(rogue_inventory_get_count(2)==0);
    rogue_persistence_load_player_stats();
    assert(rogue_inventory_get_count(2)==7);
    assert(rogue_inventory_get_count(5)==11);
    printf("INVENTORY_PERSIST_OK a=%d b=%d\n", rogue_inventory_get_count(2), rogue_inventory_get_count(5));
    return 0;
}
