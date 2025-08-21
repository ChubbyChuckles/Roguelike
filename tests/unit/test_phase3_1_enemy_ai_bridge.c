/**
 * Phase 3.1 Enemy Integration ↔ AI System Bridge Unit Tests
 *
 * Comprehensive test suite validating:
 * - Enemy spawn event handling for AI behavior tree activation (3.1.1)
 * - AI state synchronization with enemy integration registry (3.1.2)
 * - Enemy death event propagation for AI system cleanup (3.1.3)
 * - Enemy modifier application hooks for AI decision making (3.1.4)
 * - AI behavior intensity scaling based on enemy difficulty (3.1.5)
 * - Enemy group coordination through shared AI blackboards (3.1.6)
 * - AI performance metrics integration with enemy analytics (3.1.7)
 */

#include "core/integration/config_version.h"
#include "core/integration/enemy_ai_bridge.h"
#include "entities/enemy.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Test results tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message)                                                            \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            printf("FAIL: %s\n", message);                                                         \
            return 0;                                                                              \
        }                                                                                          \
    } while (0)

#define RUN_TEST(test_func, test_name)                                                             \
    do                                                                                             \
    {                                                                                              \
        printf("Running %s... ", test_name);                                                       \
        tests_run++;                                                                               \
        if (test_func())                                                                           \
        {                                                                                          \
            printf("PASS\n");                                                                      \
            tests_passed++;                                                                        \
        }                                                                                          \
    } while (0)

// Mock event bus for testing
typedef struct MockEventBus
{
    int initialized;
} MockEventBus;

static MockEventBus mock_event_bus = {1};

// === Test Fixtures ===

static RogueEnemy* create_test_enemy(uint32_t id, bool is_boss, bool is_elite, int tier)
{
    RogueEnemy* enemy = (RogueEnemy*) calloc(1, sizeof(RogueEnemy));
    if (!enemy)
        return NULL;

    enemy->base.pos.x = 100.0f + (id * 10);
    enemy->base.pos.y = 200.0f + (id * 10);
    enemy->health = 100;
    enemy->max_health = 100;
    enemy->level = 5;
    enemy->alive = 1;
    enemy->boss_flag = is_boss ? 1 : 0;
    enemy->elite_flag = is_elite ? 1 : 0;
    enemy->tier_id = tier;
    enemy->ai_bt_enabled = 0;
    enemy->ai_tree = NULL;
    enemy->ai_bt_state = NULL;
    enemy->ai_intensity = ROGUE_AI_INTENSITY_NORMAL;
    enemy->ai_intensity_score = 0.0f;
    enemy->ai_intensity_cooldown_ms = 0.0f;

    return enemy;
}

static void destroy_test_enemy(RogueEnemy* enemy)
{
    if (enemy)
    {
        free(enemy);
    }
}

// === Phase 3.1.1: Enemy Spawn Event Handling Tests ===

static int test_enemy_spawn_ai_activation(void)
{
    // Initialize bridge
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create test enemy
    RogueEnemy* enemy = create_test_enemy(1, false, false, 2);
    TEST_ASSERT(enemy != NULL, "Test enemy should be created");

    // Handle spawn event
    int result = rogue_enemy_ai_bridge_handle_spawn(&bridge, enemy);
    TEST_ASSERT(result == 1, "Spawn handling should succeed");

    // Verify AI activation
    TEST_ASSERT(enemy->ai_bt_enabled == 1, "AI behavior tree should be enabled");
    TEST_ASSERT(enemy->ai_intensity == ROGUE_AI_INTENSITY_NORMAL,
                "AI intensity should be set correctly");

    // Verify metrics updated
    const RogueAIPerformanceMetrics* metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics->total_ai_agents == 1, "Total AI agents should be 1");
    TEST_ASSERT(metrics->active_behavior_trees == 1, "Active behavior trees should be 1");
    TEST_ASSERT(metrics->intensity_metrics[ROGUE_AI_INTENSITY_NORMAL].agent_count == 1,
                "Normal intensity agent count should be 1");

    // Cleanup
    destroy_test_enemy(enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_boss_enemy_spawn_intensity(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create boss enemy
    RogueEnemy* boss_enemy = create_test_enemy(2, true, false, 5);
    TEST_ASSERT(boss_enemy != NULL, "Boss enemy should be created");

    // Handle spawn
    int result = rogue_enemy_ai_bridge_handle_spawn(&bridge, boss_enemy);
    TEST_ASSERT(result == 1, "Boss spawn handling should succeed");

    // Verify boss gets maximum intensity
    TEST_ASSERT(boss_enemy->ai_intensity == ROGUE_AI_INTENSITY_BOSS,
                "Boss should get BOSS intensity");

    // Verify metrics
    const RogueAIPerformanceMetrics* metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics->intensity_metrics[ROGUE_AI_INTENSITY_BOSS].agent_count == 1,
                "Boss intensity agent count should be 1");

    destroy_test_enemy(boss_enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_elite_enemy_spawn_intensity(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create elite enemy
    RogueEnemy* elite_enemy = create_test_enemy(3, false, true, 3);
    TEST_ASSERT(elite_enemy != NULL, "Elite enemy should be created");

    // Handle spawn
    int result = rogue_enemy_ai_bridge_handle_spawn(&bridge, elite_enemy);
    TEST_ASSERT(result == 1, "Elite spawn handling should succeed");

    // Verify elite gets elite intensity
    TEST_ASSERT(elite_enemy->ai_intensity == ROGUE_AI_INTENSITY_ELITE,
                "Elite should get ELITE intensity");

    destroy_test_enemy(elite_enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Phase 3.1.2: AI State Synchronization Tests ===

static int test_ai_state_synchronization(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create and setup enemy
    RogueEnemy* enemy = create_test_enemy(4, false, false, 2);
    rogue_enemy_ai_bridge_handle_spawn(&bridge, enemy);

    // Test state sync
    int result = rogue_enemy_ai_bridge_sync_state(&bridge, enemy);
    TEST_ASSERT(result == 1, "State synchronization should succeed for AI-enabled enemy");

    // Verify sync counters
    TEST_ASSERT(bridge.state_sync_requests >= 2,
                "Sync requests should be tracked"); // spawn + explicit sync
    TEST_ASSERT(bridge.successful_syncs >= 2, "Successful syncs should be tracked");

    destroy_test_enemy(enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_sync_all_states(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create multiple enemies
    RogueEnemy* enemies[3];
    for (int i = 0; i < 3; i++)
    {
        enemies[i] = create_test_enemy(10 + i, false, false, 2);
        rogue_enemy_ai_bridge_handle_spawn(&bridge, enemies[i]);
    }

    // Sync all states
    uint32_t synced_count = rogue_enemy_ai_bridge_sync_all_states(&bridge);
    TEST_ASSERT(synced_count == 3, "Should sync all 3 active AI agents");

    // Cleanup
    for (int i = 0; i < 3; i++)
    {
        destroy_test_enemy(enemies[i]);
    }
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Phase 3.1.3: Enemy Death Event Handling Tests ===

static int test_enemy_death_cleanup(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create and spawn enemy
    RogueEnemy* enemy = create_test_enemy(5, false, false, 2);
    rogue_enemy_ai_bridge_handle_spawn(&bridge, enemy);

    // Verify initial state
    const RogueAIPerformanceMetrics* metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics->total_ai_agents == 1, "Should have 1 AI agent before death");

    // Handle death
    int result = rogue_enemy_ai_bridge_handle_death(&bridge, enemy);
    TEST_ASSERT(result == 1, "Death handling should succeed");

    // Verify AI cleanup
    TEST_ASSERT(enemy->ai_bt_enabled == 0, "AI should be disabled after death");
    TEST_ASSERT(enemy->ai_tree == NULL, "Behavior tree should be cleaned up");
    TEST_ASSERT(enemy->ai_bt_state == NULL, "AI state should be cleaned up");

    // Verify metrics updated
    metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics->total_ai_agents == 0, "Should have 0 AI agents after death");
    TEST_ASSERT(metrics->active_behavior_trees == 0,
                "Should have 0 active behavior trees after death");

    destroy_test_enemy(enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Phase 3.1.4: Modifier Application Hook Tests ===

static int test_modifier_application_hooks(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create and spawn enemy
    RogueEnemy* enemy = create_test_enemy(6, false, false, 2);
    rogue_enemy_ai_bridge_handle_spawn(&bridge, enemy);

    RogueEnemyAIIntensity initial_intensity = (RogueEnemyAIIntensity) enemy->ai_intensity;

    // Apply berserker modifier
    int result = rogue_enemy_ai_bridge_apply_modifier_hook(&bridge, enemy, 2);
    TEST_ASSERT(result == 1, "Modifier application should succeed");
    TEST_ASSERT(enemy->ai_intensity == ROGUE_AI_INTENSITY_AGGRESSIVE,
                "Berserker modifier should increase intensity to aggressive");

    // Apply cautious modifier
    result = rogue_enemy_ai_bridge_apply_modifier_hook(&bridge, enemy, 3);
    TEST_ASSERT(result == 1, "Modifier application should succeed");
    TEST_ASSERT(enemy->ai_intensity == ROGUE_AI_INTENSITY_PASSIVE,
                "Cautious modifier should decrease intensity to passive");

    destroy_test_enemy(enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Phase 3.1.5: AI Behavior Intensity Scaling Tests ===

static int test_intensity_scaling_by_difficulty(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    RogueEnemy* enemy = create_test_enemy(7, false, false, 2);

    // Test different difficulty levels
    RogueEnemyAIIntensity intensity;

    intensity = rogue_enemy_ai_bridge_scale_intensity(&bridge, enemy, 10); // Low difficulty
    TEST_ASSERT(intensity == ROGUE_AI_INTENSITY_PASSIVE, "Low difficulty should be passive");

    intensity = rogue_enemy_ai_bridge_scale_intensity(&bridge, enemy, 30); // Normal difficulty
    TEST_ASSERT(intensity == ROGUE_AI_INTENSITY_NORMAL, "Normal difficulty should be normal");

    intensity = rogue_enemy_ai_bridge_scale_intensity(&bridge, enemy, 50); // High difficulty
    TEST_ASSERT(intensity == ROGUE_AI_INTENSITY_AGGRESSIVE, "High difficulty should be aggressive");

    intensity = rogue_enemy_ai_bridge_scale_intensity(&bridge, enemy, 70); // Elite difficulty
    TEST_ASSERT(intensity == ROGUE_AI_INTENSITY_ELITE, "Elite difficulty should be elite");

    intensity = rogue_enemy_ai_bridge_scale_intensity(&bridge, enemy, 90); // Boss difficulty
    TEST_ASSERT(intensity == ROGUE_AI_INTENSITY_BOSS, "Boss difficulty should be boss");

    destroy_test_enemy(enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_boss_flag_overrides_difficulty(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create boss enemy with low difficulty
    RogueEnemy* boss = create_test_enemy(8, true, false, 1);

    // Even with low difficulty, boss flag should override
    RogueEnemyAIIntensity intensity = rogue_enemy_ai_bridge_scale_intensity(&bridge, boss, 10);
    TEST_ASSERT(intensity == ROGUE_AI_INTENSITY_BOSS, "Boss flag should override low difficulty");

    destroy_test_enemy(boss);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_dynamic_intensity_updates(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    RogueEnemy* enemy = create_test_enemy(9, false, false, 2);
    rogue_enemy_ai_bridge_handle_spawn(&bridge, enemy);

    RogueEnemyAIIntensity initial_intensity = (RogueEnemyAIIntensity) enemy->ai_intensity;

    // Simulate low health condition
    enemy->health = 20;                     // 20% health
    enemy->ai_intensity_score = 6.0f;       // High intensity score
    enemy->ai_intensity_cooldown_ms = 0.0f; // No cooldown

    // Update intensity
    int result = rogue_enemy_ai_bridge_update_intensity(&bridge, enemy);
    TEST_ASSERT(result == 1, "Intensity should update due to low health and high score");
    TEST_ASSERT(enemy->ai_intensity > initial_intensity, "Intensity should increase");
    TEST_ASSERT(enemy->ai_intensity_cooldown_ms > 0, "Cooldown should be set after update");

    destroy_test_enemy(enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Phase 3.1.6: Enemy Group Coordination Tests ===

static int test_group_creation_and_management(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create group members
    uint32_t enemy_ids[4] = {1001, 1002, 1003, 1004};
    uint32_t formation_pattern = 101; // Formation pattern ID

    // Create group
    uint32_t group_id =
        rogue_enemy_ai_bridge_create_group(&bridge, enemy_ids, 4, formation_pattern);
    TEST_ASSERT(group_id != 0, "Group creation should succeed");
    TEST_ASSERT(bridge.active_group_count == 1, "Active group count should be 1");

    // Find the created group
    RogueEnemyGroup* group = NULL;
    for (uint8_t i = 0; i < 64; i++)
    {
        if (bridge.groups[i].group_id == group_id)
        {
            group = &bridge.groups[i];
            break;
        }
    }

    TEST_ASSERT(group != NULL, "Created group should be findable");
    TEST_ASSERT(group->member_count == 4, "Group should have 4 members");
    TEST_ASSERT(group->formation_pattern == formation_pattern, "Formation pattern should be set");
    TEST_ASSERT(group->shared_blackboard != NULL, "Shared blackboard should be created");

    // Test group coordination update
    int result = rogue_enemy_ai_bridge_update_group_coordination(&bridge, group_id);
    TEST_ASSERT(result == 1, "Group coordination update should succeed");
    TEST_ASSERT(group->last_update_time > 0, "Last update time should be set");

    // Destroy group
    result = rogue_enemy_ai_bridge_destroy_group(&bridge, group_id);
    TEST_ASSERT(result == 1, "Group destruction should succeed");
    TEST_ASSERT(bridge.active_group_count == 0, "Active group count should be 0 after destruction");

    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_group_member_death_handling(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create enemies and group
    RogueEnemy* enemies[3];
    uint32_t enemy_ids[3];

    for (int i = 0; i < 3; i++)
    {
        enemies[i] = create_test_enemy(2000 + i, false, false, 2);
        enemy_ids[i] = 2000 + i; // Use the ID directly instead of casting pointer
        rogue_enemy_ai_bridge_handle_spawn(&bridge, enemies[i]);
    }

    uint32_t group_id = rogue_enemy_ai_bridge_create_group(&bridge, enemy_ids, 3, 201);
    TEST_ASSERT(group_id != 0, "Group creation should succeed");

    // Find group and verify initial state
    RogueEnemyGroup* group = NULL;
    for (uint8_t i = 0; i < 64; i++)
    {
        if (bridge.groups[i].group_id == group_id)
        {
            group = &bridge.groups[i];
            break;
        }
    }
    TEST_ASSERT(group->member_count == 3, "Group should initially have 3 members");

    // Kill one enemy
    int result = rogue_enemy_ai_bridge_handle_death(&bridge, enemies[1]);
    TEST_ASSERT(result == 1, "Death handling should succeed");

    // Group should be updated to remove dead member
    TEST_ASSERT(group->member_count == 2, "Group should have 2 members after one death");

    // Kill remaining enemies
    rogue_enemy_ai_bridge_handle_death(&bridge, enemies[0]);
    rogue_enemy_ai_bridge_handle_death(&bridge, enemies[2]);

    // Group should be automatically destroyed when empty
    TEST_ASSERT(bridge.active_group_count == 0, "Group should be destroyed when all members die");

    // Cleanup
    for (int i = 0; i < 3; i++)
    {
        destroy_test_enemy(enemies[i]);
    }
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Phase 3.1.7: Performance Metrics Integration Tests ===

static int test_performance_metrics_tracking(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Get initial metrics
    const RogueAIPerformanceMetrics* metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics != NULL, "Metrics should be accessible");
    TEST_ASSERT(metrics->total_ai_agents == 0, "Initial agent count should be 0");

    // Create several enemies with different intensities
    RogueEnemy* normal_enemy = create_test_enemy(3001, false, false, 2);
    RogueEnemy* elite_enemy = create_test_enemy(3002, false, true, 4);
    RogueEnemy* boss_enemy = create_test_enemy(3003, true, false, 5);

    rogue_enemy_ai_bridge_handle_spawn(&bridge, normal_enemy);
    rogue_enemy_ai_bridge_handle_spawn(&bridge, elite_enemy);
    rogue_enemy_ai_bridge_handle_spawn(&bridge, boss_enemy);

    // Check updated metrics
    metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics->total_ai_agents == 3, "Should have 3 AI agents");
    TEST_ASSERT(metrics->active_behavior_trees == 3, "Should have 3 active behavior trees");
    TEST_ASSERT(metrics->intensity_metrics[ROGUE_AI_INTENSITY_NORMAL].agent_count == 1,
                "Should have 1 normal intensity agent");
    TEST_ASSERT(metrics->intensity_metrics[ROGUE_AI_INTENSITY_ELITE].agent_count == 1,
                "Should have 1 elite intensity agent");
    TEST_ASSERT(metrics->intensity_metrics[ROGUE_AI_INTENSITY_BOSS].agent_count == 1,
                "Should have 1 boss intensity agent");

    // Test metrics update
    rogue_enemy_ai_bridge_update_metrics(&bridge, 0.016f); // 60 FPS frame time

    // Test metrics reset
    rogue_enemy_ai_bridge_reset_metrics(&bridge);
    metrics = rogue_enemy_ai_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics->total_ai_agents == 3, "Agent count should persist after metrics reset");
    TEST_ASSERT(metrics->performance_warnings == 0, "Performance warnings should be reset");

    // Cleanup
    destroy_test_enemy(normal_enemy);
    destroy_test_enemy(elite_enemy);
    destroy_test_enemy(boss_enemy);
    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_performance_threshold_checking(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Set low thresholds for testing
    bridge.max_tick_time_warning_us = 100.0f;
    bridge.max_tick_time_error_us = 200.0f;
    bridge.max_agents_per_frame = 2;

    // Normal operation should pass
    bridge.metrics.average_tick_time_us = 50.0f;
    bridge.metrics.peak_tick_time_us = 80.0f;
    bridge.metrics.total_ai_agents = 1;

    int threshold_exceeded = rogue_enemy_ai_bridge_check_performance_thresholds(&bridge);
    TEST_ASSERT(threshold_exceeded == 0, "Normal performance should not exceed thresholds");

    // Exceed warning threshold
    bridge.metrics.average_tick_time_us = 150.0f;
    threshold_exceeded = rogue_enemy_ai_bridge_check_performance_thresholds(&bridge);
    TEST_ASSERT(threshold_exceeded == 1, "Warning threshold should be exceeded");

    // Exceed error threshold
    bridge.metrics.peak_tick_time_us = 250.0f;
    threshold_exceeded = rogue_enemy_ai_bridge_check_performance_thresholds(&bridge);
    TEST_ASSERT(threshold_exceeded == 1, "Error threshold should be exceeded");

    // Exceed agent count threshold
    bridge.metrics.average_tick_time_us = 50.0f; // Reset to normal
    bridge.metrics.peak_tick_time_us = 80.0f;
    bridge.metrics.total_ai_agents = 5; // Exceeds limit of 2
    threshold_exceeded = rogue_enemy_ai_bridge_check_performance_thresholds(&bridge);
    TEST_ASSERT(threshold_exceeded == 1, "Agent count threshold should be exceeded");

    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Debug and Validation Tests ===

static int test_debug_functionality(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Test debug logging
    rogue_enemy_ai_bridge_set_debug_logging(&bridge, true);
    TEST_ASSERT(bridge.debug_logging_enabled == true, "Debug logging should be enabled");

    rogue_enemy_ai_bridge_set_debug_logging(&bridge, false);
    TEST_ASSERT(bridge.debug_logging_enabled == false, "Debug logging should be disabled");

    // Test debug status
    char debug_buffer[512];
    int chars_written =
        rogue_enemy_ai_bridge_get_debug_status(&bridge, debug_buffer, sizeof(debug_buffer));
    TEST_ASSERT(chars_written > 0, "Debug status should return formatted text");
    TEST_ASSERT(strstr(debug_buffer, "Enemy-AI Bridge Status") != NULL,
                "Debug status should contain header");

    // Test validation
    int is_valid = rogue_enemy_ai_bridge_validate(&bridge);
    TEST_ASSERT(is_valid == 1, "Bridge should be valid after initialization");

    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

static int test_error_handling(void)
{
    RogueEnemyAIBridge bridge;

    // Test NULL pointer handling
    TEST_ASSERT(rogue_enemy_ai_bridge_init(NULL, (struct RogueEventBus*) &mock_event_bus) == 0,
                "Should handle NULL bridge pointer");

    TEST_ASSERT(rogue_enemy_ai_bridge_handle_spawn(NULL, NULL) == 0,
                "Should handle NULL parameters gracefully");

    TEST_ASSERT(rogue_enemy_ai_bridge_handle_death(NULL, NULL) == 0,
                "Should handle NULL parameters gracefully");

    TEST_ASSERT(rogue_enemy_ai_bridge_sync_state(NULL, NULL) == 0,
                "Should handle NULL parameters gracefully");

    // Test uninitialized bridge
    memset(&bridge, 0, sizeof(bridge));
    TEST_ASSERT(rogue_enemy_ai_bridge_handle_spawn(&bridge, NULL) == 0,
                "Should handle uninitialized bridge");

    return 1;
}

// === Stress Testing ===

static int test_large_group_management(void)
{
    RogueEnemyAIBridge bridge;
    TEST_ASSERT(rogue_enemy_ai_bridge_init(&bridge, (struct RogueEventBus*) &mock_event_bus) == 1,
                "Bridge should initialize successfully");

    // Create maximum number of groups (64)
    uint32_t group_ids[64];
    uint32_t enemy_ids[4] = {4001, 4002, 4003, 4004};

    for (int i = 0; i < 64; i++)
    {
        group_ids[i] = rogue_enemy_ai_bridge_create_group(&bridge, enemy_ids, 4, 300 + i);
        TEST_ASSERT(group_ids[i] != 0, "Group creation should succeed within limits");
    }

    TEST_ASSERT(bridge.active_group_count == 64, "Should have maximum number of groups");

    // Try to create one more group (should fail)
    uint32_t overflow_group = rogue_enemy_ai_bridge_create_group(&bridge, enemy_ids, 4, 999);
    TEST_ASSERT(overflow_group == 0, "Group creation should fail when at maximum capacity");

    // Destroy all groups
    for (int i = 0; i < 64; i++)
    {
        int result = rogue_enemy_ai_bridge_destroy_group(&bridge, group_ids[i]);
        TEST_ASSERT(result == 1, "Group destruction should succeed");
    }

    TEST_ASSERT(bridge.active_group_count == 0, "All groups should be destroyed");

    rogue_enemy_ai_bridge_shutdown(&bridge);
    return 1;
}

// === Test Suite Runner ===

void run_phase3_1_tests(void)
{
    printf("\n=== Running Phase 3.1 Enemy Integration ↔ AI System Bridge Tests ===\n\n");

    // Initialize configuration system for event type registration
    rogue_config_version_init(".");

    // Phase 3.1.1: Enemy spawn event handling
    RUN_TEST(test_enemy_spawn_ai_activation, "Enemy spawn AI activation (3.1.1)");
    RUN_TEST(test_boss_enemy_spawn_intensity, "Boss enemy spawn intensity (3.1.1)");
    RUN_TEST(test_elite_enemy_spawn_intensity, "Elite enemy spawn intensity (3.1.1)");

    // Phase 3.1.2: AI state synchronization
    RUN_TEST(test_ai_state_synchronization, "AI state synchronization (3.1.2)");
    RUN_TEST(test_sync_all_states, "Sync all AI states (3.1.2)");

    // Phase 3.1.3: Enemy death event handling
    RUN_TEST(test_enemy_death_cleanup, "Enemy death AI cleanup (3.1.3)");

    // Phase 3.1.4: Modifier application hooks
    RUN_TEST(test_modifier_application_hooks, "Modifier application hooks (3.1.4)");

    // Phase 3.1.5: AI behavior intensity scaling
    RUN_TEST(test_intensity_scaling_by_difficulty, "Intensity scaling by difficulty (3.1.5)");
    RUN_TEST(test_boss_flag_overrides_difficulty, "Boss flag overrides difficulty (3.1.5)");
    RUN_TEST(test_dynamic_intensity_updates, "Dynamic intensity updates (3.1.5)");

    // Phase 3.1.6: Enemy group coordination
    RUN_TEST(test_group_creation_and_management, "Group creation and management (3.1.6)");
    RUN_TEST(test_group_member_death_handling, "Group member death handling (3.1.6)");

    // Phase 3.1.7: Performance metrics integration
    RUN_TEST(test_performance_metrics_tracking, "Performance metrics tracking (3.1.7)");
    RUN_TEST(test_performance_threshold_checking, "Performance threshold checking (3.1.7)");

    // Debug and error handling
    RUN_TEST(test_debug_functionality, "Debug functionality");
    RUN_TEST(test_error_handling, "Error handling");

    // Stress testing
    RUN_TEST(test_large_group_management, "Large group management stress test");

    // Cleanup
    rogue_config_version_shutdown();

    printf("\n=== Phase 3.1 Test Summary ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run)
    {
        printf("\n[SUCCESS] All Phase 3.1 Enemy Integration ↔ AI System Bridge tests passed!\n");
    }
    else
    {
        printf("\n[FAILURE] Some Phase 3.1 tests failed. Check output above for details.\n");
    }
}

// Main function for standalone test execution
int main(void)
{
    run_phase3_1_tests();
    return (tests_passed == tests_run) ? 0 : 1;
}
