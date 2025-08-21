/* test_enemy_difficulty_phase1_ext.c - Extended Phase 1 tests (remaining 1.3,1.4,1.6,1.7 components)
 * Validates:
 *  - Attribute derivation curves monotonic-ish with level
 *  - Biome parameter registry override (hp penalty/gain reflection in reward/ΔL unaffected)
 *  - ΔL severity classification mapping
 *  - TTK estimation produces reasonable ordering across tiers & ΔL grid sample
 */
#include <stdio.h>
#include <math.h>
#include "core/enemy/enemy_difficulty_scaling.h"

static int test_attributes_monotonic(void){
    RogueEnemyDerivedAttributes prev={0};
    for(int L=1; L<=60; ++L){
        RogueEnemyDerivedAttributes cur; if(rogue_enemy_compute_attributes(L,L,ROGUE_ENEMY_TIER_NORMAL,-1,&cur)!=0){ printf("FAIL attrib compute\n"); return 1; }
        if(L>1){
            if(cur.crit_chance < prev.crit_chance - 0.005f){ printf("FAIL crit non-monotonic L=%d\n", L); return 1; }
            if(cur.phys_resist < prev.phys_resist - 0.01f){ printf("FAIL phys non-monotonic L=%d\n", L); return 1; }
        }
        prev=cur;
    }
    return 0;
}

static int test_biome_override(void){
    RogueEnemyDifficultyParams custom = *rogue_enemy_difficulty_params_current();
    custom.d_def *= 2.f; /* exaggerate downward penalty */
    if(rogue_enemy_difficulty_register_biome_params(7, &custom)!=0){ printf("FAIL biome register\n"); return 1; }
    RogueEnemyFinalStats base, bio;
    if(rogue_enemy_compute_final_stats(30,30,ROGUE_ENEMY_TIER_VETERAN,&base)!=0){ printf("FAIL base stats\n"); return 1; }
    if(rogue_enemy_compute_final_stats_biome(30,30,ROGUE_ENEMY_TIER_VETERAN,7,&bio)!=0){ printf("FAIL biome stats\n"); return 1; }
    /* Equal level => relative mult 1, so change only visible if future biome scaling applied; currently they match. */
    if(fabs(base.hp - bio.hp) > 0.01f){ printf("FAIL biome mismatch future placeholder\n"); return 1; }
    return 0;
}

static int test_delta_classification(void){
    if(rogue_enemy_difficulty_classify_delta(20,20)!=ROGUE_DLVL_EQUAL) return 1;
    if(rogue_enemy_difficulty_classify_delta(28,20)<ROGUE_DLVL_MAJOR) return 1;
    if(rogue_enemy_difficulty_classify_delta(35,20)!=ROGUE_DLVL_TRIVIAL && rogue_enemy_difficulty_classify_delta(35,20)!=ROGUE_DLVL_DOMINANCE) return 1;
    if(rogue_enemy_difficulty_classify_delta(20,28)<ROGUE_DLVL_MODERATE) return 1;
    return 0;
}

static int test_ttk_estimation(void){
    float t1 = rogue_enemy_estimate_ttk_seconds(20,20,ROGUE_ENEMY_TIER_NORMAL,-1,150.f);
    float t2 = rogue_enemy_estimate_ttk_seconds(20,20,ROGUE_ENEMY_TIER_ELITE,-1,150.f);
    float t3 = rogue_enemy_estimate_ttk_seconds(10,20,ROGUE_ENEMY_TIER_ELITE,-1,150.f); /* player under-level => higher TTK */
    if(!(t2 > t1*1.3f)) { printf("FAIL elite vs normal TTK ratio\n"); return 1; }
    if(!(t3 > t2*1.1f)) { printf("FAIL underlevel TTK ordering\n"); return 1; }
    return 0;
}

int main(void){
    if(test_attributes_monotonic()!=0) return 1;
    if(test_biome_override()!=0) return 1;
    if(test_delta_classification()!=0) return 1;
    if(test_ttk_estimation()!=0) return 1;
    printf("OK test_enemy_difficulty_phase1_ext\n");
    return 0;
}
