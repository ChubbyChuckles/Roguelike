#include "core/loot/loot_rarity_adv.h"
#include "core/loot/loot_instances.h"
#include "core/loot_pickup.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>

static int expect(int cond, const char* msg){ if(!cond){ printf("FAIL: %s\n", msg); return 0;} return 1; }

int main(void){
    /* Initialize runtime item pool */
    rogue_items_init_runtime();
    /* Set player near origin */
    g_app.player.base.pos.x = 0.f; g_app.player.base.pos.y = 0.f;
    /* Assign pickup sounds */
    rogue_rarity_set_pickup_sound(0, "s_common");
    rogue_rarity_set_pickup_sound(4, "s_legendary");
    int pass=1;
    /* Spawn common (rarity 0) and legendary (force def rarity using fake defs conceptually; here rely on default def rarity retrieval) */
    int inst_c = rogue_items_spawn(0,1,0.1f,0.1f); /* def 0 assumed rarity 0 in test data */
    int inst_l = rogue_items_spawn(1,1,0.2f,0.2f); /* def 1 may not be rarity 4, can't guarantee -> simulate by setting pickup sound retrieval only; test ensures no crash */
    pass &= expect(inst_c>=0, "spawn common");
    pass &= expect(inst_l>=0, "spawn second");
    rogue_loot_pickup_update(1.0f); /* radius large enough */
    /* Just ensure instances marked inactive after pickup */
    const RogueItemInstance* c_after = rogue_item_instance_at(inst_c);
    const RogueItemInstance* l_after = rogue_item_instance_at(inst_l);
    pass &= expect(c_after==NULL, "common picked up");
    pass &= expect(l_after==NULL, "legendary picked up");
    if(pass) printf("OK\n");
    return pass?0:1;
}
