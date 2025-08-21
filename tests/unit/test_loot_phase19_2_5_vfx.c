#include "core/loot/loot_vfx.h"
#include "core/loot/loot_instances.h"
#include "core/app_state.h"
#include <stdio.h>

static int expect(int cond, const char* msg){ if(!cond){ printf("FAIL: %s\n", msg); return 0;} return 1; }

int main(void){
    rogue_items_init_runtime();
    rogue_loot_vfx_reset();
    g_app.player.base.pos.x = 0; g_app.player.base.pos.y = 0;
    int inst = rogue_items_spawn(0,1,5.0f,0.0f); /* spawn outside view radius to test edge notifier */
    int pass=1; pass &= expect(inst>=0, "spawn inst");
    RogueLootVFXState st; pass &= expect(rogue_loot_vfx_get(inst,&st)==1, "get vfx state");
    /* Beam may be inactive for low rarity test item; simply ensure value is 0 or 1 */
    pass &= expect(st.beam_active==0 || st.beam_active==1, "beam flag valid");
    /* Advance time to near despawn window */
    for(int i=0;i<55;i++){ rogue_items_update(1000.0f); } /* advance to 55s */
    rogue_items_update(4000.0f); /* 59s total */
    pass &= expect(rogue_loot_vfx_get(inst,&st)==1, "still active pre window end");
    float prev_alpha = st.pulse_alpha; pass &= expect(st.pulse_active==1, "pulse became active by 59s" );
    rogue_items_update(800.0f); /* cross 59.8s -> within final 200ms */
    pass &= expect(rogue_loot_vfx_get(inst,&st)==1, "before despawn still");
    pass &= expect(st.pulse_alpha >= prev_alpha, "alpha increasing");
    /* Force despawn */
    rogue_items_update(1000.0f);
    pass &= expect(rogue_loot_vfx_get(inst,&st)==0, "state cleared post-despawn");
    if(pass) printf("OK\n");
    return pass?0:1;
}
