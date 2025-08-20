#include <stdio.h>
#include <assert.h>
#include "core/progression_mastery.h"

int main(void){
    if(rogue_progression_mastery_init()<0){ printf("init_fail\n"); return 1; }
    int sid=7;
    double v = rogue_progression_mastery_add_xp(sid, 50.0);
    if(v < 50.0){ printf("add_small_fail\n"); return 2; }
    int r = rogue_progression_mastery_get_rank(sid);
    if(r != 0){ printf("rank_should_be_0\n"); return 3; }
    rogue_progression_mastery_add_xp(sid, 60.0); /* crosses 100 threshold */
    r = rogue_progression_mastery_get_rank(sid);
    if(r < 1){ printf("rank_not_increased\n"); return 4; }
    double xp = rogue_progression_mastery_get_xp(sid);
    if(xp <= 0.0){ printf("xp_nonpos\n"); return 5; }
    /* Ensure threshold growth: simulate many ranks */
    rogue_progression_mastery_add_xp(sid, 100000.0);
    r = rogue_progression_mastery_get_rank(sid);
    if(r < 5){ printf("rank_growth_unexpected=%d\n", r); return 6; }
    rogue_progression_mastery_shutdown();
    printf("progression_mastery: OK ranks=%d xp=%.1f\n", r, xp);
    return 0;
}
