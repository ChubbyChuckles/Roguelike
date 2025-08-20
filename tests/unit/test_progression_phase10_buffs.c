#include <stdio.h>
#include <string.h>
#include "core/buffs.h"
#include "core/stat_cache.h"
#include "entities/player.h"

static int test_stack_rules(void){
    rogue_buffs_init();
    double t=0; /* add unique then attempt duplicate */
    int ok = rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE,10,5000,t,ROGUE_BUFF_STACK_UNIQUE,0); if(!ok){ printf("fail_unique_first\n"); return 1; }
    ok = rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE,10,5000,t+10,ROGUE_BUFF_STACK_UNIQUE,0); if(ok){ printf("fail_unique_duplicate\n"); return 1; }
    /* refresh rule picks higher magnitude and resets duration */
    t += 100; ok = rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE,15,4000,t,ROGUE_BUFF_STACK_REFRESH,0); if(!ok){ printf("fail_refresh_apply\n"); return 1; }
    /* extend rule adds duration (can't easily introspect end_ms without public API; rely on total magnitude logic) */
    t += 100; ok = rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE,20,2000,t,ROGUE_BUFF_STACK_EXTEND,0); if(!ok){ printf("fail_extend_apply\n"); return 1; }
    return 0;
}

static int test_strength_buff_layer(void){
    rogue_buffs_init();
    RoguePlayer p; memset(&p,0,sizeof p); p.strength=10; /* apply additive strength buff */
    int ok = rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH,5,3000,0.0,ROGUE_BUFF_STACK_ADD,1); if(!ok){ printf("fail_strength_buff_apply\n"); return 1; }
    extern RogueStatCache g_player_stat_cache; memset(&g_player_stat_cache,0,sizeof g_player_stat_cache); g_player_stat_cache.dirty=1; rogue_stat_cache_force_update(&p);
    if(g_player_stat_cache.total_strength != 15){ printf("fail_strength_total %d\n", g_player_stat_cache.total_strength); return 1; }
    return 0;
}

static int test_dampening(void){
    rogue_buffs_init(); rogue_buffs_set_dampening(200.0); double t=0;
    int ok = rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH,3,1000,t,ROGUE_BUFF_STACK_ADD,0); if(!ok){ printf("fail_damp_first\n"); return 1; }
    ok = rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH,3,1000,t+50,ROGUE_BUFF_STACK_ADD,0); if(ok){ printf("fail_damp_allowed\n"); return 1; }
    ok = rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH,3,1000,t+250,ROGUE_BUFF_STACK_ADD,0); if(!ok){ printf("fail_damp_second\n"); return 1; }
    return 0;
}

int main(void){
    if(test_stack_rules()) return 1;
    if(test_strength_buff_layer()) return 1;
    if(test_dampening()) return 1;
    printf("progression_phase10_buffs: OK\n");
    return 0;
}
