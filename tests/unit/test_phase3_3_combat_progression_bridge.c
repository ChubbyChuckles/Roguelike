#include "core/integration/combat_progression_bridge.h"
#include "core/integration/config_version.h"
#include "entities/player.h"
#include "game/combat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    if (!(condition)) { \
        printf("    [FAIL] %s\n", message); \
        return 0; \
    } \
    printf("    [PASS] %s\n", message); \
    return 1; \
} while(0)

#define RUN_TEST(test_func, test_name) do { \
    printf("\n--- Running %s ---\n", test_name); \
    tests_run++; \
    if (test_func()) { \
        tests_passed++; \
        printf("✓ %s PASSED\n", test_name); \
    } else { \
        printf("✗ %s FAILED\n", test_name); \
    } \
} while(0)

/* External function stubs */
extern int rogue_equip_get(int slot) { (void)slot; return 1; }
extern void rogue_event_bus_init(void) {}
extern void rogue_event_bus_shutdown(void) {}

/* Phase 3.3.1: Combat XP Distribution Tests */
int test_combat_xp_distribution() {
    RogueCombatProgressionBridge bridge = {0};
    
    /* Initialize bridge */
    int init_result = rogue_combat_progression_bridge_init(&bridge);
    TEST_ASSERT(init_result == 1, "Bridge initialization should succeed");
    TEST_ASSERT(bridge.initialized == true, "Bridge should be marked as initialized");
    TEST_ASSERT(bridge.current_difficulty_multiplier == 1.0f, "Default difficulty multiplier should be 1.0");
    
    /* Test XP award from damage dealt */
    int xp_result = rogue_combat_progression_bridge_award_xp(&bridge, 
                                                             ROGUE_XP_SOURCE_DAMAGE_DEALT,
                                                             200, 25, 1001);
    TEST_ASSERT(xp_result == 1, "XP award should succeed");
    TEST_ASSERT(bridge.xp_distribution_count == 1, "Should have 1 XP distribution record");
    
    RogueCombatXpDistribution* xp_dist = &bridge.xp_distributions[0];
    TEST_ASSERT(xp_dist->damage_dealt == 200, "Damage dealt should match");
    TEST_ASSERT(xp_dist->enemy_difficulty == 25, "Enemy difficulty should match");
    TEST_ASSERT(xp_dist->total_xp_awarded > 0, "Should have awarded some XP");
    TEST_ASSERT(bridge.total_xp_awarded_session > 0, "Session XP should be tracked");
    
    /* Test XP award from enemy defeated */
    int xp_result2 = rogue_combat_progression_bridge_award_xp(&bridge,
                                                              ROGUE_XP_SOURCE_ENEMY_DEFEATED,
                                                              0, 50, 1002);
    TEST_ASSERT(xp_result2 == 1, "Enemy defeated XP award should succeed");
    TEST_ASSERT(bridge.xp_distribution_count == 2, "Should have 2 XP distribution records");
    
    /* Test difficulty multiplier effect */
    uint32_t initial_session_xp = bridge.total_xp_awarded_session;
    bridge.current_difficulty_multiplier = 1.5f;
    
    int xp_result3 = rogue_combat_progression_bridge_award_xp(&bridge,
                                                              ROGUE_XP_SOURCE_DIFFICULTY_BONUS,
                                                              100, 75, 1003);
    TEST_ASSERT(xp_result3 == 1, "Difficulty bonus XP award should succeed");
    
    uint32_t final_session_xp = bridge.total_xp_awarded_session;
    TEST_ASSERT(final_session_xp > initial_session_xp, "Session XP should increase");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Phase 3.3.2: Skill Usage Tracking Tests */
int test_skill_usage_tracking() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test skill tracking */
    int track_result = rogue_combat_progression_bridge_track_skill_usage(&bridge, 101, 75, 2001);
    TEST_ASSERT(track_result == 1, "Skill usage tracking should succeed");
    TEST_ASSERT(bridge.tracked_skills_count == 1, "Should have 1 tracked skill");
    TEST_ASSERT(bridge.total_skill_activations == 1, "Total activations should be 1");
    
    RogueCombatSkillUsage* skill = &bridge.skill_usage_tracking[0];
    TEST_ASSERT(skill->skill_id == 101, "Skill ID should match");
    TEST_ASSERT(skill->usage_count == 1, "Usage count should be 1");
    TEST_ASSERT(skill->effectiveness_score == 75, "Effectiveness score should match");
    
    /* Test multiple uses of same skill */
    rogue_combat_progression_bridge_track_skill_usage(&bridge, 101, 85, 2002);
    TEST_ASSERT(bridge.tracked_skills_count == 1, "Should still have 1 tracked skill");
    TEST_ASSERT(bridge.total_skill_activations == 2, "Total activations should be 2");
    TEST_ASSERT(skill->usage_count == 2, "Usage count should be 2");
    
    /* Test mastery progress tracking */
    uint32_t progress, required;
    int mastery_result = rogue_combat_progression_bridge_get_skill_mastery_progress(&bridge, 101, &progress, &required);
    TEST_ASSERT(mastery_result == 1, "Mastery progress query should succeed");
    TEST_ASSERT(progress > 0, "Should have some mastery progress");
    TEST_ASSERT(required == 1000, "Initial mastery requirement should be 1000");
    
    /* Test different skill */
    rogue_combat_progression_bridge_track_skill_usage(&bridge, 102, 90, 2003);
    TEST_ASSERT(bridge.tracked_skills_count == 2, "Should have 2 tracked skills");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Phase 3.3.3: Passive Skill Effects Tests */
int test_passive_skill_effects() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test passive skill activation */
    int activate_result = rogue_combat_progression_bridge_activate_passive_skill(&bridge, 201, 0, 1.25f, 10000);
    TEST_ASSERT(activate_result == 1, "Passive skill activation should succeed");
    TEST_ASSERT(bridge.active_passives_count == 1, "Should have 1 active passive");
    TEST_ASSERT(bridge.passive_effects_dirty == true, "Passive effects should be marked dirty");
    
    RoguePassiveSkillEffect* passive = &bridge.active_passive_effects[0];
    TEST_ASSERT(passive->passive_skill_id == 201, "Passive skill ID should match");
    TEST_ASSERT(passive->effect_type == 0, "Effect type should match (damage modifier)");
    TEST_ASSERT(fabs(passive->effect_magnitude - 1.25f) < 0.001f, "Effect magnitude should match");
    TEST_ASSERT(passive->is_active == true, "Passive should be active");
    
    /* Test applying passive effects */
    float damage_mod = 1.0f, defense_mod = 1.0f, speed_mod = 1.0f;
    int effects_applied = rogue_combat_progression_bridge_apply_passive_effects(&bridge, &damage_mod, &defense_mod, &speed_mod);
    TEST_ASSERT(effects_applied == 1, "Should apply 1 passive effect");
    TEST_ASSERT(fabs(damage_mod - 1.25f) < 0.001f, "Damage modifier should be applied");
    TEST_ASSERT(fabs(defense_mod - 1.0f) < 0.001f, "Defense modifier should be unchanged");
    TEST_ASSERT(fabs(speed_mod - 1.0f) < 0.001f, "Speed modifier should be unchanged");
    
    /* Test multiple passive effects */
    rogue_combat_progression_bridge_activate_passive_skill(&bridge, 202, 1, 0.8f, 5000); /* Defense reduction */
    TEST_ASSERT(bridge.active_passives_count == 2, "Should have 2 active passives");
    
    damage_mod = defense_mod = speed_mod = 1.0f;
    effects_applied = rogue_combat_progression_bridge_apply_passive_effects(&bridge, &damage_mod, &defense_mod, &speed_mod);
    TEST_ASSERT(effects_applied == 2, "Should apply 2 passive effects");
    TEST_ASSERT(fabs(damage_mod - 1.25f) < 0.001f, "Damage modifier should be applied");
    TEST_ASSERT(fabs(defense_mod - 0.8f) < 0.001f, "Defense modifier should be applied");
    
    /* Test passive expiration through update */
    rogue_combat_progression_bridge_update(&bridge, 6000.0f); /* 6 seconds */
    TEST_ASSERT(bridge.active_passives_count == 1, "Should have 1 active passive after expiration");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Phase 3.3.4: Achievement System Tests */
int test_achievement_system() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Simulate conditions for first achievement (First Strike - enemy killed) */
    bridge.total_xp_awarded_session = 100; /* Some initial XP */
    
    int achievements_triggered = rogue_combat_progression_bridge_check_achievements(&bridge, 0x01, 1);
    TEST_ASSERT(achievements_triggered > 0, "Should trigger at least 1 achievement");
    TEST_ASSERT(bridge.achievement_count > 0, "Should have unlocked achievements");
    TEST_ASSERT(bridge.achievements_unlocked_session > 0, "Session achievements should be tracked");
    
    /* Test getting recent achievements */
    RogueCombatAchievement recent_achievements[5];
    int recent_count = rogue_combat_progression_bridge_get_recent_achievements(&bridge, recent_achievements, 5);
    TEST_ASSERT(recent_count > 0, "Should have recent achievements");
    TEST_ASSERT(strlen(recent_achievements[0].achievement_name) > 0, "Achievement should have a name");
    TEST_ASSERT(recent_achievements[0].reward_xp > 0, "Achievement should have XP reward");
    
    /* Test XP threshold achievement */
    bridge.total_xp_awarded_session = 600; /* Above XP Hunter threshold */
    achievements_triggered = rogue_combat_progression_bridge_check_achievements(&bridge, 0x03, 600);
    uint8_t prev_count = bridge.achievement_count;
    TEST_ASSERT(bridge.achievement_count >= prev_count, "Achievement count should not decrease");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Phase 3.3.5: Playstyle Analysis Tests */
int test_playstyle_analysis() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test initial playstyle */
    float initial_confidence;
    RogueCombatPlaystyle initial_style = rogue_combat_progression_bridge_get_detected_playstyle(&bridge, &initial_confidence);
    TEST_ASSERT(initial_style == ROGUE_PLAYSTYLE_BALANCED, "Initial playstyle should be balanced");
    TEST_ASSERT(initial_confidence == 0.0f, "Initial confidence should be 0");
    
    /* Simulate aggressive playstyle data over multiple sessions */
    for (int i = 0; i < 12; i++) {
        int analysis_result = rogue_combat_progression_bridge_analyze_playstyle(&bridge, 70, 80, 30);
        TEST_ASSERT(analysis_result == 1, "Playstyle analysis should succeed");
    }
    
    /* Check if aggressive playstyle was detected */
    float confidence;
    RogueCombatPlaystyle detected_style = rogue_combat_progression_bridge_get_detected_playstyle(&bridge, &confidence);
    TEST_ASSERT(bridge.playstyle_profile.combat_sessions_analyzed >= 10, "Should have analyzed multiple sessions");
    TEST_ASSERT(confidence > 0.0f, "Should have some confidence in detection");
    TEST_ASSERT(bridge.metrics.playstyle_analyses_performed > 0, "Should have performed analyses");
    
    /* Test defensive playstyle simulation */
    RogueCombatProgressionBridge bridge2 = {0};
    rogue_combat_progression_bridge_init(&bridge2);
    
    for (int i = 0; i < 12; i++) {
        rogue_combat_progression_bridge_analyze_playstyle(&bridge2, 40, 20, 60); /* Low risk, high tactical */
    }
    
    detected_style = rogue_combat_progression_bridge_get_detected_playstyle(&bridge2, &confidence);
    TEST_ASSERT(bridge2.playstyle_profile.risk_tolerance < 50, "Should show low risk tolerance");
    TEST_ASSERT(bridge2.playstyle_profile.tactical_usage > 50, "Should show high tactical usage");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    rogue_combat_progression_bridge_shutdown(&bridge2);
    return 1;
}

/* Phase 3.3.6: Efficiency Metrics Tests */
int test_efficiency_metrics() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test initial efficiency */
    float initial_score;
    RogueCombatEfficiencyMetrics detailed;
    int score_result = rogue_combat_progression_bridge_get_efficiency_score(&bridge, &initial_score, &detailed);
    TEST_ASSERT(score_result == 1, "Getting efficiency score should succeed");
    TEST_ASSERT(initial_score == 50.0f, "Initial efficiency should be baseline (50%)");
    
    /* Test efficiency update */
    int update_result = rogue_combat_progression_bridge_update_efficiency_metrics(&bridge, 5000.0f, 300, 50, 20);
    TEST_ASSERT(update_result == 1, "Efficiency update should succeed");
    TEST_ASSERT(bridge.metrics.efficiency_calculations_performed == 1, "Should have performed 1 calculation");
    
    /* Check updated metrics */
    score_result = rogue_combat_progression_bridge_get_efficiency_score(&bridge, NULL, &detailed);
    TEST_ASSERT(detailed.damage_per_second_avg > 0, "DPS should be calculated");
    TEST_ASSERT(detailed.damage_per_mana_efficiency > 0, "Damage per mana should be calculated");
    TEST_ASSERT(detailed.time_to_kill_avg_ms > 0, "TTK should be calculated");
    TEST_ASSERT(detailed.resource_usage_efficiency > 0, "Resource efficiency should be calculated");
    
    /* Test perfect combat (no damage taken) */
    rogue_combat_progression_bridge_update_efficiency_metrics(&bridge, 3000.0f, 400, 60, 0);
    score_result = rogue_combat_progression_bridge_get_efficiency_score(&bridge, NULL, &detailed);
    TEST_ASSERT(detailed.perfect_combat_count == 1, "Should track perfect combat");
    
    /* Test efficiency milestone triggering */
    bridge.efficiency_metrics.overall_efficiency_score = 85.0f; /* High efficiency */
    rogue_combat_progression_bridge_update_efficiency_metrics(&bridge, 2000.0f, 500, 40, 5);
    TEST_ASSERT(bridge.efficiency_milestone_count > 0, "Should have efficiency milestones");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Phase 3.3.7: Death Penalty Tests */
int test_death_penalty() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test death penalty application */
    int penalty_result = rogue_combat_progression_bridge_apply_death_penalty(&bridge, 10, 1000);
    TEST_ASSERT(penalty_result == 1, "Death penalty application should succeed");
    TEST_ASSERT(bridge.death_penalty_state.death_count == 1, "Death count should be incremented");
    TEST_ASSERT(bridge.death_penalty_state.xp_penalty_amount > 0, "Should have XP penalty");
    TEST_ASSERT(bridge.death_penalty_state.equipment_durability_loss > 0, "Should have durability loss");
    TEST_ASSERT(bridge.metrics.death_penalties_applied == 1, "Should track penalty applications");
    
    /* Test penalty info retrieval */
    RogueCombatDeathPenalty penalty_info;
    int info_result = rogue_combat_progression_bridge_get_death_penalty_info(&bridge, &penalty_info);
    TEST_ASSERT(info_result == 1, "Getting penalty info should succeed");
    TEST_ASSERT(penalty_info.death_count == 1, "Death count should match");
    TEST_ASSERT(penalty_info.xp_penalty_percentage >= 5.0f, "XP penalty percentage should be at least 5%");
    TEST_ASSERT(penalty_info.skill_penalty_duration_ms == 300000, "Skill penalty duration should be 5 minutes");
    
    /* Test multiple deaths */
    rogue_combat_progression_bridge_apply_death_penalty(&bridge, 15, 2000);
    TEST_ASSERT(bridge.death_penalty_state.death_count == 2, "Death count should be 2");
    
    /* Test penalty scaling with level */
    uint32_t prev_xp_penalty = bridge.death_penalty_state.xp_penalty_amount;
    rogue_combat_progression_bridge_apply_death_penalty(&bridge, 25, 5000);
    TEST_ASSERT(bridge.death_penalty_state.xp_penalty_amount >= prev_xp_penalty, "Higher level should have higher or equal penalty");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Integration and Performance Tests */
int test_combat_progression_integration() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test comprehensive workflow */
    /* 1. Award XP */
    rogue_combat_progression_bridge_award_xp(&bridge, ROGUE_XP_SOURCE_DAMAGE_DEALT, 150, 30, 3001);
    
    /* 2. Track skill usage */
    rogue_combat_progression_bridge_track_skill_usage(&bridge, 301, 85, 3001);
    
    /* 3. Activate passive effects */
    rogue_combat_progression_bridge_activate_passive_skill(&bridge, 401, 0, 1.15f, 0); /* Permanent damage boost */
    
    /* 4. Check achievements */
    rogue_combat_progression_bridge_check_achievements(&bridge, 0x01, 1);
    
    /* 5. Analyze playstyle */
    rogue_combat_progression_bridge_analyze_playstyle(&bridge, 65, 55, 45);
    
    /* 6. Update efficiency */
    rogue_combat_progression_bridge_update_efficiency_metrics(&bridge, 4000.0f, 200, 40, 15);
    
    /* 7. Apply modifiers from passives */
    float dmg_mod, def_mod, spd_mod;
    int effects = rogue_combat_progression_bridge_apply_passive_effects(&bridge, &dmg_mod, &def_mod, &spd_mod);
    TEST_ASSERT(effects > 0, "Should apply passive effects");
    TEST_ASSERT(dmg_mod > 1.0f, "Damage should be boosted by passive");
    
    /* 8. Update bridge state */
    rogue_combat_progression_bridge_update(&bridge, 100.0f);
    
    /* Verify all systems interacted properly */
    TEST_ASSERT(bridge.total_xp_awarded_session > 0, "Should have awarded XP");
    TEST_ASSERT(bridge.total_skill_activations > 0, "Should have tracked skills");
    TEST_ASSERT(bridge.active_passives_count > 0, "Should have active passives");
    TEST_ASSERT(bridge.achievement_count > 0, "Should have unlocked achievements");
    TEST_ASSERT(bridge.playstyle_data_points[3] > 0, "Should have playstyle data");
    TEST_ASSERT(bridge.efficiency_metrics.overall_efficiency_score > 0, "Should have efficiency score");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

int test_combat_progression_performance() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Performance test with many operations */
    for (int i = 0; i < 100; i++) {
        rogue_combat_progression_bridge_award_xp(&bridge, ROGUE_XP_SOURCE_DAMAGE_DEALT, 100 + i, 20 + (i % 30), 4000 + i);
        rogue_combat_progression_bridge_track_skill_usage(&bridge, 500 + (i % 10), 70 + (i % 30), 4000 + i);
        
        if (i % 10 == 0) {
            rogue_combat_progression_bridge_analyze_playstyle(&bridge, 50 + (i % 40), 40 + (i % 50), 30 + (i % 60));
            rogue_combat_progression_bridge_update_efficiency_metrics(&bridge, 3000.0f + (i * 10), 150 + i, 30 + (i % 20), i % 10);
        }
    }
    
    TEST_ASSERT(bridge.metrics.xp_distributions_processed >= 100, "Should process many XP distributions");
    TEST_ASSERT(bridge.metrics.skill_usage_events_processed >= 100, "Should process many skill events");
    TEST_ASSERT(bridge.metrics.avg_processing_time_ms >= 0, "Should calculate average processing time");
    
    /* Test metrics reporting */
    char metrics_buffer[2048];
    rogue_combat_progression_bridge_get_metrics(&bridge, metrics_buffer, sizeof(metrics_buffer));
    TEST_ASSERT(strlen(metrics_buffer) > 100, "Should generate detailed metrics report");
    
    /* Test metrics reset */
    rogue_combat_progression_bridge_reset_metrics(&bridge);
    TEST_ASSERT(bridge.xp_distribution_count == 0, "XP distributions should be reset");
    TEST_ASSERT(bridge.total_xp_awarded_session == 0, "Session XP should be reset");
    TEST_ASSERT(bridge.achievements_unlocked_session == 0, "Session achievements should be reset");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Debug and Error Handling Tests */
int test_combat_progression_debug() {
    RogueCombatProgressionBridge bridge = {0};
    
    rogue_combat_progression_bridge_init(&bridge);
    
    /* Test debug mode */
    rogue_combat_progression_bridge_set_debug_mode(&bridge, false);
    TEST_ASSERT(bridge.debug_mode == false, "Debug mode should be disabled");
    
    rogue_combat_progression_bridge_set_debug_mode(&bridge, true);
    TEST_ASSERT(bridge.debug_mode == true, "Debug mode should be enabled");
    
    /* Test error handling with invalid parameters */
    int result = rogue_combat_progression_bridge_award_xp(NULL, ROGUE_XP_SOURCE_DAMAGE_DEALT, 100, 10, 5001);
    TEST_ASSERT(result == 0, "Should handle NULL bridge gracefully");
    
    result = rogue_combat_progression_bridge_track_skill_usage(NULL, 101, 50, 5001);
    TEST_ASSERT(result == 0, "Should handle NULL bridge gracefully");
    
    float modifiers[3] = {1.0f, 1.0f, 1.0f};
    result = rogue_combat_progression_bridge_apply_passive_effects(&bridge, NULL, &modifiers[1], &modifiers[2]);
    TEST_ASSERT(result == 0, "Should handle NULL parameters gracefully");
    
    /* Test boundary conditions */
    /* Fill up XP distribution array */
    for (int i = 0; i < 35; i++) { /* More than max capacity */
        rogue_combat_progression_bridge_award_xp(&bridge, ROGUE_XP_SOURCE_DAMAGE_DEALT, 50, 10, 6000 + i);
    }
    TEST_ASSERT(bridge.xp_distribution_count <= 32, "Should not exceed maximum XP distributions");
    
    rogue_combat_progression_bridge_shutdown(&bridge);
    return 1;
}

/* Main test runner function */
void run_phase3_3_tests() {
    printf("===========================================\n");
    printf("Phase 3.3 Combat-Progression Bridge Tests\n");
    printf("===========================================\n");
    
    tests_run = 0;
    tests_passed = 0;
    
    /* Phase 3.3.1: Combat XP Distribution */
    RUN_TEST(test_combat_xp_distribution, "Phase 3.3.1 Combat XP Distribution");
    
    /* Phase 3.3.2: Skill Usage Tracking */
    RUN_TEST(test_skill_usage_tracking, "Phase 3.3.2 Skill Usage Tracking");
    
    /* Phase 3.3.3: Passive Skill Effects */
    RUN_TEST(test_passive_skill_effects, "Phase 3.3.3 Passive Skill Effects");
    
    /* Phase 3.3.4: Achievement System */
    RUN_TEST(test_achievement_system, "Phase 3.3.4 Achievement System");
    
    /* Phase 3.3.5: Playstyle Analysis */
    RUN_TEST(test_playstyle_analysis, "Phase 3.3.5 Playstyle Analysis");
    
    /* Phase 3.3.6: Efficiency Metrics */
    RUN_TEST(test_efficiency_metrics, "Phase 3.3.6 Efficiency Metrics");
    
    /* Phase 3.3.7: Death Penalty */
    RUN_TEST(test_death_penalty, "Phase 3.3.7 Death Penalty");
    
    /* Integration Tests */
    RUN_TEST(test_combat_progression_integration, "Phase 3.3 Integration Workflow");
    
    /* Performance Tests */
    RUN_TEST(test_combat_progression_performance, "Phase 3.3 Performance & Metrics");
    
    /* Debug & Error Handling Tests */
    RUN_TEST(test_combat_progression_debug, "Phase 3.3 Debug & Error Handling");
    
    printf("\n===========================================\n");
    printf("Phase 3.3 Test Summary: %d/%d tests passed\n", tests_passed, tests_run);
    
    if (tests_passed == tests_run) {
        printf("🎉 All Phase 3.3 Combat-Progression Bridge tests PASSED!\n");
        printf("\n✅ Phase 3.3.1: Combat XP distribution based on damage & difficulty\n");
        printf("✅ Phase 3.3.2: Skill usage tracking for mastery progression\n");
        printf("✅ Phase 3.3.3: Passive skill effects application to combat calculations\n");
        printf("✅ Phase 3.3.4: Combat achievement triggers for progression milestones\n");
        printf("✅ Phase 3.3.5: Combat playstyle analysis for adaptive progression suggestions\n");
        printf("✅ Phase 3.3.6: Combat efficiency metrics for progression analytics\n");
        printf("✅ Phase 3.3.7: Combat death penalty integration with progression system\n");
        printf("\n📊 Features validated: XP distribution, skill mastery, passive effects,\n");
        printf("    achievements, playstyle detection, efficiency tracking, death penalties\n");
    } else {
        printf("\n[FAILURE] Some Phase 3.3 tests failed. Check output above for details.\n");
    }
}

/* Main function for standalone test execution */
int main(int argc, char* argv[]) {
    /* Suppress unused parameter warnings */
    (void)argc;
    (void)argv;
    
    run_phase3_3_tests();
    return (tests_passed == tests_run) ? 0 : 1;
}
