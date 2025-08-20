/* test_enemy_difficulty_phase4.c - Adaptive difficulty Phase 4 tests
 * Validates:
 *  - Scalar remains 1.0 when disabled
 *  - Upward pressure raises scalar toward max when player overperforms (fast kills, low damage)
 *  - Downward pressure lowers scalar when player struggles (slow kills, high intake, potion spam)
 *  - Bounds respected and relaxation toward 1.0 when neutral
 */
#include <stdio.h>
#include <math.h>
#include "core/enemy_adaptive.h"
#include "core/enemy_difficulty_scaling.h"

static int approx(float a,float b,float eps){ float d=a-b; if(d<0) d=-d; return d<=eps; }

static int test_disable_behavior(void){
    rogue_enemy_adaptive_reset();
    rogue_enemy_adaptive_set_enabled(0);
    for(int i=0;i<10;i++){ rogue_enemy_adaptive_tick(1.0f); }
    if(!approx(rogue_enemy_adaptive_scalar(),1.0f,0.0001f)){ printf("FAIL adaptive disabled scalar drift\n"); return 1; }
    return 0;
}

static int test_upward_pressure(void){
    rogue_enemy_adaptive_reset();
    /* Simulate several very fast kills (<< target) with low damage intake */
    for(int k=0;k<12;k++){
        rogue_enemy_adaptive_submit_kill(2.0f); /* fast */
        rogue_enemy_adaptive_submit_player_damage(1.0f, 5.0f); /* very low dps in */
        rogue_enemy_adaptive_tick(1.0f);
    }
    float s = rogue_enemy_adaptive_scalar();
    if(s < 1.01f){ printf("FAIL upward pressure scalar not increased (%.3f)\n", s); return 1; }
    if(s > ROGUE_ENEMY_ADAPTIVE_MAX_SCALAR + 0.001f){ printf("FAIL upward pressure exceeded max (%.3f)\n", s); return 1; }
    return 0;
}

static int test_downward_pressure(void){
    rogue_enemy_adaptive_reset();
    /* Simulate slow kills, high damage & potion usage */
    for(int k=0;k<14;k++){
        rogue_enemy_adaptive_submit_kill(12.0f); /* slow */
        rogue_enemy_adaptive_submit_player_damage(50.0f, 2.0f); /* high intake */
        if(k%3==0) rogue_enemy_adaptive_submit_potion_used();
        if(k%5==0) rogue_enemy_adaptive_submit_player_death();
        rogue_enemy_adaptive_tick(1.0f);
    }
    float s = rogue_enemy_adaptive_scalar();
    if(s > 0.99f){ printf("FAIL downward pressure scalar not decreased (%.3f)\n", s); return 1; }
    if(s < ROGUE_ENEMY_ADAPTIVE_MIN_SCALAR - 0.001f){ printf("FAIL downward pressure below min (%.3f)\n", s); return 1; }
    return 0;
}

static int test_relaxation(void){
    rogue_enemy_adaptive_reset();
    /* First push up */
    for(int k=0;k<10;k++){ rogue_enemy_adaptive_submit_kill(2.5f); rogue_enemy_adaptive_tick(1.0f); }
    if(rogue_enemy_adaptive_scalar() <= 1.0f){ printf("FAIL pre-relax push failed\n"); return 1; }
    /* Now neutral period */
    float before = rogue_enemy_adaptive_scalar();
    /* Advance time beyond active window and let neutral relaxation pull scalar toward 1 */
    for(int k=0;k<40;k++){ rogue_enemy_adaptive_tick(1.0f); }
    float after = rogue_enemy_adaptive_scalar();
    if(!(after < before && after > 1.0f - 0.05f)){ printf("FAIL relaxation not trending toward 1 (before=%.3f after=%.3f)\n", before, after); return 1; }
    return 0;
}

int main(void){
    if(test_disable_behavior()!=0){ printf("stage: disable_behavior failed (scalar=%.3f)\n", rogue_enemy_adaptive_scalar()); return 1; }
    if(test_upward_pressure()!=0){ printf("stage: upward_pressure failed (scalar=%.3f)\n", rogue_enemy_adaptive_scalar()); return 1; }
    if(test_downward_pressure()!=0){ printf("stage: downward_pressure failed (scalar=%.3f)\n", rogue_enemy_adaptive_scalar()); return 1; }
    if(test_relaxation()!=0){ printf("stage: relaxation failed (scalar=%.3f)\n", rogue_enemy_adaptive_scalar()); return 1; }
    printf("OK test_enemy_difficulty_phase4\n");
    return 0;
}
