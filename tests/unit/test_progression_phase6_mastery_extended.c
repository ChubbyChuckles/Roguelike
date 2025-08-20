#include <stdio.h>
#include "core/progression_mastery.h"

/* Phase 6.2-6.5 extended mastery tests: ring points, bonus tiers, decay */
int main(void){
    if(rogue_mastery_init(0,1)<0){ printf("init_fail\n"); return 1; }
    unsigned int t=0; /* ms */
    /* Add XP to three skills at different magnitudes */
    for(int i=0;i<5;i++){ rogue_mastery_add_xp(0,120,t); t+=10; }
    for(int i=0;i<20;i++){ rogue_mastery_add_xp(1,90,t); t+=5; }
    for(int i=0;i<3;i++){ rogue_mastery_add_xp(2,300,t); t+=7; }
    unsigned short r0 = rogue_mastery_rank(0);
    unsigned short r1 = rogue_mastery_rank(1);
    unsigned short r2 = rogue_mastery_rank(2);
    if(r0==0||r1==0||r2==0){ printf("rank_zero r0=%u r1=%u r2=%u\n",r0,r1,r2); return 2; }
    float b0 = rogue_mastery_bonus_scalar(0);
    float b1 = rogue_mastery_bonus_scalar(1);
    float b2 = rogue_mastery_bonus_scalar(2);
    if(!(b2>=b0 && b0>=1.0f)){ printf("bonus_order b0=%.2f b1=%.2f b2=%.2f\n",b0,b1,b2); return 3; }
    /* Force skill 1 to reach ring unlock rank */
    while(rogue_mastery_rank(1) < 5){ rogue_mastery_add_xp(1,200,t); t+=20; }
    while(rogue_mastery_rank(2) < 5){ rogue_mastery_add_xp(2,200,t); t+=20; }
    int rings = rogue_mastery_minor_ring_points();
    if(rings < 2){ printf("ring_points_fail=%d\n", rings); return 4; }
    /* Simulate inactivity decay on skill 2 */
    unsigned long long xp_pre = rogue_mastery_xp(2);
    rogue_mastery_update(120000); /* +120s -> beyond grace (60s) plus 4 decay windows */
    unsigned long long xp_post = rogue_mastery_xp(2);
    if(!(xp_post < xp_pre)){ printf("decay_fail pre=%llu post=%llu\n", xp_pre, xp_post); return 5; }
    printf("progression_phase6_mastery_extended: OK r0=%u r1=%u r2=%u rings=%d decay_loss=%llu\n", r0,r1,r2,rings,(unsigned long long)(xp_pre - xp_post));
    return 0;
}
