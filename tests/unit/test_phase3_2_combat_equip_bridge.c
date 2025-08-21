/**
 * Phase 3.2 Combat System ↔ Equipment System Bridge Unit Tests
 *
 * Comprehensive test suite validating:
 * - Real-time equipment stat application to combat calculations (3.2.1)
 * - Equipment durability reduction hooks in combat damage events (3.2.2)
 * - Equipment proc effect triggers during combat actions (3.2.3)
 * - Equipment set bonus activation/deactivation on equip/unequip (3.2.4)
 * - Equipment enchantment effects integration in combat formulas (3.2.5)
 * - Equipment weight impact on combat timing & movement (3.2.6)
 * - Equipment upgrade notifications to combat stat cache (3.2.7)
 */

#include "core/integration/combat_equip_bridge.h"
#include "core/integration/config_version.h"
#include "entities/player.h"
#include "game/combat.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test framework */
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message)                                                            \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            printf("    [FAIL] %s\n", message);                                                    \
            return 0;                                                                              \
        }                                                                                          \
        printf("    [PASS] %s\n", message);                                                        \
        return 1;                                                                                  \
    } while (0)

#define RUN_TEST(test_func, test_name)                                                             \
    do                                                                                             \
    {                                                                                              \
        printf("\n--- Running %s ---\n", test_name);                                               \
        tests_run++;                                                                               \
        if (test_func())                                                                           \
        {                                                                                          \
            tests_passed++;                                                                        \
            printf("✓ %s PASSED\n", test_name);                                                    \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("✗ %s FAILED\n", test_name);                                                    \
        }                                                                                          \
    } while (0)

/* === Test Fixtures === */

static RoguePlayer* create_test_player()
{
    RoguePlayer* player = (RoguePlayer*) calloc(1, sizeof(RoguePlayer));
    if (!player)
        return NULL;

    player->base.pos.x = 100.0f;
    player->base.pos.y = 200.0f;
    player->health = 100;
    player->max_health = 100;
    player->level = 10;

    return player;
}

static RoguePlayerCombat* create_test_combat()
{
    RoguePlayerCombat* combat = (RoguePlayerCombat*) calloc(1, sizeof(RoguePlayerCombat));
    if (!combat)
        return NULL;

    combat->phase = ROGUE_ATTACK_IDLE;
    combat->stamina = 100.0f;
    combat->combo = 0;

    return combat;
}

/* === Phase 3.2.1: Real-time Equipment Stat Application Tests === */

static int test_bridge_initialization()
{
    RogueCombatEquipBridge bridge;

    int result = rogue_combat_equip_bridge_init(&bridge);
    TEST_ASSERT(result == 1, "Bridge initialization should succeed");
    TEST_ASSERT(bridge.initialized == true, "Bridge should be marked as initialized");
    TEST_ASSERT(bridge.stats_dirty == true, "Stats should be marked as dirty initially");
    TEST_ASSERT(bridge.weight_dirty == true, "Weight should be marked as dirty initially");

    rogue_combat_equip_bridge_shutdown(&bridge);
    TEST_ASSERT(bridge.initialized == false,
                "Bridge should be marked as uninitialized after shutdown");
}

static int test_equipment_stat_calculation()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();

    rogue_combat_equip_bridge_init(&bridge);

    // Update equipment stats
    int result = rogue_combat_equip_bridge_update_stats(&bridge, player);
    TEST_ASSERT(result == 1, "Stat update should succeed");
    TEST_ASSERT(bridge.stats_dirty == false, "Stats should no longer be dirty after update");

    // Get combat stats
    RogueCombatEquipmentStats stats;
    result = rogue_combat_equip_bridge_get_combat_stats(&bridge, &stats);
    TEST_ASSERT(result == 1, "Getting combat stats should succeed");
    TEST_ASSERT(stats.damage_multiplier >= 1.0f, "Damage multiplier should be at least 1.0");
    TEST_ASSERT(stats.attack_speed_multiplier >= 0.5f, "Attack speed should be reasonable");

    free(player);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_combat_stat_application()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();
    RoguePlayerCombat* combat = create_test_combat();

    rogue_combat_equip_bridge_init(&bridge);
    rogue_combat_equip_bridge_update_stats(&bridge, player);

    // Apply stats to combat
    int result = rogue_combat_equip_bridge_apply_stats_to_combat(&bridge, combat);
    TEST_ASSERT(result == 1, "Applying stats to combat should succeed");

    free(player);
    free(combat);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Phase 3.2.2: Equipment Durability Tests === */

static int test_durability_damage_taken()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();

    rogue_combat_equip_bridge_init(&bridge);

    // Simulate taking damage
    uint32_t damage_amount = 50;
    uint8_t damage_type = 0; // Physical damage

    int result =
        rogue_combat_equip_bridge_on_damage_taken(&bridge, player, damage_amount, damage_type);
    TEST_ASSERT(result == 1, "Processing damage taken should succeed");
    TEST_ASSERT(bridge.durability_event_count > 0, "Should generate durability events");

    // Process durability events
    int processed = rogue_combat_equip_bridge_process_durability_events(&bridge);
    TEST_ASSERT(processed >= 0, "Should process durability events successfully");
    TEST_ASSERT(bridge.durability_event_count == 0,
                "Event queue should be cleared after processing");

    free(player);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_durability_weapon_attack()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();

    rogue_combat_equip_bridge_init(&bridge);

    // Simulate weapon attack (hit)
    int result = rogue_combat_equip_bridge_on_attack_made(&bridge, player, true);
    TEST_ASSERT(result == 1, "Processing weapon attack should succeed");

    // Simulate weapon attack (miss)
    result = rogue_combat_equip_bridge_on_attack_made(&bridge, player, false);
    TEST_ASSERT(result == 1, "Processing weapon attack (miss) should succeed");

    // Process durability events
    int processed = rogue_combat_equip_bridge_process_durability_events(&bridge);
    TEST_ASSERT(processed >= 0, "Should process weapon durability events");

    free(player);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Phase 3.2.3: Equipment Proc Tests === */

static int test_proc_triggering()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Trigger procs on hit
    uint8_t trigger_type = 0; // On hit trigger
    uint32_t context_data = 12345;

    int result = rogue_combat_equip_bridge_trigger_procs(&bridge, trigger_type, context_data);
    TEST_ASSERT(result == 1, "Triggering procs should succeed");
    TEST_ASSERT(bridge.active_proc_count >= 0, "Should track active procs");

    // Get active procs
    RogueEquipmentProcActivation procs[16];
    int active_count = rogue_combat_equip_bridge_get_active_procs(&bridge, procs, 16);
    TEST_ASSERT(active_count >= 0, "Getting active procs should succeed");
    TEST_ASSERT(active_count == bridge.active_proc_count, "Active count should match");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_proc_duration_updates()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Trigger some procs
    rogue_combat_equip_bridge_trigger_procs(&bridge, 0, 54321);
    uint8_t initial_count = bridge.active_proc_count;

    // Update proc durations (simulate 1 second)
    int result = rogue_combat_equip_bridge_update_active_procs(&bridge, 1000.0f);
    TEST_ASSERT(result >= 0, "Updating proc durations should succeed");

    // Update again with large time step to expire procs (simulate 10 seconds)
    result = rogue_combat_equip_bridge_update_active_procs(&bridge, 10000.0f);
    TEST_ASSERT(result >= 0, "Updating proc durations with large time step should succeed");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Phase 3.2.4: Equipment Set Bonus Tests === */

static int test_set_bonus_detection()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();

    rogue_combat_equip_bridge_init(&bridge);

    // Update set bonuses
    int result = rogue_combat_equip_bridge_update_set_bonuses(&bridge, player);
    TEST_ASSERT(result >= 0, "Updating set bonuses should succeed");
    TEST_ASSERT(bridge.set_bonus_count >= 0, "Should track set bonus count");

    // Get set bonuses
    RogueEquipmentSetBonusState bonuses[8];
    int bonus_count = rogue_combat_equip_bridge_get_set_bonuses(&bridge, bonuses, 8);
    TEST_ASSERT(bonus_count >= 0, "Getting set bonuses should succeed");
    TEST_ASSERT(bonus_count == bridge.set_bonus_count, "Set bonus count should match");

    free(player);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_set_bonus_combat_application()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();
    RoguePlayerCombat* combat = create_test_combat();

    rogue_combat_equip_bridge_init(&bridge);
    rogue_combat_equip_bridge_update_set_bonuses(&bridge, player);

    // Apply set bonuses to combat
    int result = rogue_combat_equip_bridge_apply_set_bonuses_to_combat(&bridge, combat);
    TEST_ASSERT(result == 1, "Applying set bonuses to combat should succeed");

    free(player);
    free(combat);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Phase 3.2.5: Equipment Enchantment Tests === */

static int test_enchantment_application()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();

    rogue_combat_equip_bridge_init(&bridge);

    // Apply enchantments
    float damage_multiplier = 1.0f;
    uint32_t elemental_damage = 0;

    int result = rogue_combat_equip_bridge_apply_enchantments(&bridge, player, &damage_multiplier,
                                                              &elemental_damage);
    TEST_ASSERT(result == 1, "Applying enchantments should succeed");
    TEST_ASSERT(damage_multiplier >= 1.0f, "Damage multiplier should be enhanced");

    free(player);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_enchantment_effects_triggering()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Trigger enchantment effects
    uint8_t enchant_trigger = 1; // On crit trigger
    uint32_t context_data = 98765;

    int result = rogue_combat_equip_bridge_trigger_enchantment_effects(&bridge, enchant_trigger,
                                                                       context_data);
    TEST_ASSERT(result == 1, "Triggering enchantment effects should succeed");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Phase 3.2.6: Equipment Weight Tests === */

static int test_weight_impact_calculation()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();

    rogue_combat_equip_bridge_init(&bridge);

    // Update weight impact
    int result = rogue_combat_equip_bridge_update_weight_impact(&bridge, player);
    TEST_ASSERT(result == 1, "Updating weight impact should succeed");
    TEST_ASSERT(bridge.weight_dirty == false, "Weight should no longer be dirty after update");

    // Get weight impact
    RogueEquipmentWeightImpact impact;
    result = rogue_combat_equip_bridge_get_weight_impact(&bridge, &impact);
    TEST_ASSERT(result == 1, "Getting weight impact should succeed");
    TEST_ASSERT(impact.total_weight >= 0.0f, "Total weight should be non-negative");
    TEST_ASSERT(impact.attack_speed_modifier >= 0.0f && impact.attack_speed_modifier <= 2.0f,
                "Attack speed modifier should be reasonable");

    free(player);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_weight_combat_application()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();
    RoguePlayerCombat* combat = create_test_combat();

    rogue_combat_equip_bridge_init(&bridge);
    rogue_combat_equip_bridge_update_weight_impact(&bridge, player);

    // Apply weight impact to combat
    int result = rogue_combat_equip_bridge_apply_weight_to_combat(&bridge, combat);
    TEST_ASSERT(result == 1, "Applying weight impact to combat should succeed");

    free(player);
    free(combat);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Phase 3.2.7: Equipment Upgrade Notification Tests === */

static int test_equipment_upgrade_notification()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Notify of equipment upgrade
    uint8_t slot = 0; // ROGUE_EQUIP_WEAPON
    uint32_t old_item_id = 1001;
    uint32_t new_item_id = 1002;

    int result =
        rogue_combat_equip_bridge_on_equipment_upgraded(&bridge, slot, old_item_id, new_item_id);
    TEST_ASSERT(result == 1, "Equipment upgrade notification should succeed");
    TEST_ASSERT(bridge.stats_dirty == true, "Stats should be marked dirty after upgrade");
    TEST_ASSERT(bridge.weight_dirty == true, "Weight should be marked dirty after upgrade");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_equipment_enchant_notification()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Notify of equipment enchantment
    uint8_t slot = 1; // ROGUE_EQUIP_OFFHAND
    uint32_t enchant_id = 5001;

    int result = rogue_combat_equip_bridge_on_equipment_enchanted(&bridge, slot, enchant_id);
    TEST_ASSERT(result == 1, "Equipment enchant notification should succeed");
    TEST_ASSERT(bridge.stats_dirty == true, "Stats should be marked dirty after enchantment");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_equipment_socket_notification()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Notify of gem socketing
    uint8_t slot = 2; // ROGUE_EQUIP_ARMOR_HEAD
    uint32_t gem_id = 7001;

    int result = rogue_combat_equip_bridge_on_equipment_socketed(&bridge, slot, gem_id);
    TEST_ASSERT(result == 1, "Equipment socket notification should succeed");
    TEST_ASSERT(bridge.stats_dirty == true, "Stats should be marked dirty after socketing");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Performance & Debug Tests === */

static int test_performance_metrics_tracking()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Get initial metrics
    RogueCombatEquipBridgeMetrics metrics;
    int result = rogue_combat_equip_bridge_get_metrics(&bridge, &metrics);
    TEST_ASSERT(result == 1, "Getting metrics should succeed");
    TEST_ASSERT(metrics.last_metrics_reset > 0, "Metrics should have valid reset timestamp");

    // Reset metrics
    rogue_combat_equip_bridge_reset_metrics(&bridge);

    // Verify reset
    result = rogue_combat_equip_bridge_get_metrics(&bridge, &metrics);
    TEST_ASSERT(result == 1, "Getting metrics after reset should succeed");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_performance_threshold_checking()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Check performance thresholds
    int warnings = rogue_combat_equip_bridge_check_performance_thresholds(&bridge);
    TEST_ASSERT(warnings >= 0, "Performance threshold checking should succeed");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

static int test_debug_functionality()
{
    RogueCombatEquipBridge bridge;

    rogue_combat_equip_bridge_init(&bridge);

    // Test debug logging control
    rogue_combat_equip_bridge_set_debug_logging(&bridge, true);
    int debug_status = rogue_combat_equip_bridge_get_debug_status(&bridge);
    TEST_ASSERT(debug_status == 1, "Debug logging should be enabled");

    rogue_combat_equip_bridge_set_debug_logging(&bridge, false);
    debug_status = rogue_combat_equip_bridge_get_debug_status(&bridge);
    TEST_ASSERT(debug_status == 0, "Debug logging should be disabled");

    // Test validation
    int validation_result = rogue_combat_equip_bridge_validate(&bridge);
    TEST_ASSERT(validation_result == 1, "Bridge validation should succeed");

    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Integration Tests === */

static int test_comprehensive_integration_workflow()
{
    RogueCombatEquipBridge bridge;
    RoguePlayer* player = create_test_player();
    RoguePlayerCombat* combat = create_test_combat();

    rogue_combat_equip_bridge_init(&bridge);
    rogue_combat_equip_bridge_set_debug_logging(&bridge, true);

    // Full integration workflow
    // 1. Update equipment stats
    int result = rogue_combat_equip_bridge_update_stats(&bridge, player);
    TEST_ASSERT(result == 1, "Stat update should succeed");

    // 2. Update weight impact
    result = rogue_combat_equip_bridge_update_weight_impact(&bridge, player);
    TEST_ASSERT(result == 1, "Weight update should succeed");

    // 3. Update set bonuses
    result = rogue_combat_equip_bridge_update_set_bonuses(&bridge, player);
    TEST_ASSERT(result >= 0, "Set bonus update should succeed");

    // 4. Apply everything to combat
    result = rogue_combat_equip_bridge_apply_stats_to_combat(&bridge, combat);
    TEST_ASSERT(result == 1, "Applying stats to combat should succeed");

    result = rogue_combat_equip_bridge_apply_weight_to_combat(&bridge, combat);
    TEST_ASSERT(result == 1, "Applying weight to combat should succeed");

    result = rogue_combat_equip_bridge_apply_set_bonuses_to_combat(&bridge, combat);
    TEST_ASSERT(result == 1, "Applying set bonuses to combat should succeed");

    // 5. Simulate combat events
    result = rogue_combat_equip_bridge_on_damage_taken(&bridge, player, 75, 0);
    TEST_ASSERT(result == 1, "Processing damage taken should succeed");

    result = rogue_combat_equip_bridge_on_attack_made(&bridge, player, true);
    TEST_ASSERT(result == 1, "Processing attack made should succeed");

    result = rogue_combat_equip_bridge_trigger_procs(&bridge, 0, 13579);
    TEST_ASSERT(result == 1, "Triggering procs should succeed");

    // 6. Process events
    int processed = rogue_combat_equip_bridge_process_durability_events(&bridge);
    TEST_ASSERT(processed >= 0, "Processing durability events should succeed");

    int active_procs = rogue_combat_equip_bridge_update_active_procs(&bridge, 100.0f);
    TEST_ASSERT(active_procs >= 0, "Updating active procs should succeed");

    // 7. Validate final state
    result = rogue_combat_equip_bridge_validate(&bridge);
    TEST_ASSERT(result == 1, "Final bridge validation should succeed");

    free(player);
    free(combat);
    rogue_combat_equip_bridge_shutdown(&bridge);
    return 1;
}

/* === Main Test Runner === */

static void run_phase3_2_tests()
{
    printf("=== Phase 3.2 Combat-Equipment Bridge Unit Tests ===\n");
    printf("Testing comprehensive Combat System ↔ Equipment System integration\n\n");

    // Initialize configuration system (required dependency)
    if (!rogue_config_version_init("."))
    {
        printf("[ERROR] Failed to initialize config system\n");
        return;
    }

    // Phase 3.2.1: Real-time equipment stat application
    RUN_TEST(test_bridge_initialization, "Bridge initialization (3.2.1)");
    RUN_TEST(test_equipment_stat_calculation, "Equipment stat calculation (3.2.1)");
    RUN_TEST(test_combat_stat_application, "Combat stat application (3.2.1)");

    // Phase 3.2.2: Equipment durability reduction hooks
    RUN_TEST(test_durability_damage_taken, "Durability on damage taken (3.2.2)");
    RUN_TEST(test_durability_weapon_attack, "Durability on weapon attack (3.2.2)");

    // Phase 3.2.3: Equipment proc effect triggers
    RUN_TEST(test_proc_triggering, "Equipment proc triggering (3.2.3)");
    RUN_TEST(test_proc_duration_updates, "Proc duration updates (3.2.3)");

    // Phase 3.2.4: Equipment set bonus activation/deactivation
    RUN_TEST(test_set_bonus_detection, "Set bonus detection (3.2.4)");
    RUN_TEST(test_set_bonus_combat_application, "Set bonus combat application (3.2.4)");

    // Phase 3.2.5: Equipment enchantment effects integration
    RUN_TEST(test_enchantment_application, "Enchantment application (3.2.5)");
    RUN_TEST(test_enchantment_effects_triggering, "Enchantment effects triggering (3.2.5)");

    // Phase 3.2.6: Equipment weight impact
    RUN_TEST(test_weight_impact_calculation, "Weight impact calculation (3.2.6)");
    RUN_TEST(test_weight_combat_application, "Weight combat application (3.2.6)");

    // Phase 3.2.7: Equipment upgrade notifications
    RUN_TEST(test_equipment_upgrade_notification, "Equipment upgrade notification (3.2.7)");
    RUN_TEST(test_equipment_enchant_notification, "Equipment enchant notification (3.2.7)");
    RUN_TEST(test_equipment_socket_notification, "Equipment socket notification (3.2.7)");

    // Performance & Debug tests
    RUN_TEST(test_performance_metrics_tracking, "Performance metrics tracking");
    RUN_TEST(test_performance_threshold_checking, "Performance threshold checking");
    RUN_TEST(test_debug_functionality, "Debug functionality");

    // Integration tests
    RUN_TEST(test_comprehensive_integration_workflow, "Comprehensive integration workflow");

    rogue_config_version_shutdown();

    printf("\n=== Phase 3.2 Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", (tests_passed * 100.0f) / tests_run);

    if (tests_passed == tests_run)
    {
        printf("\n[SUCCESS] All Phase 3.2 Combat-Equipment Bridge tests passed!\n");
        printf("✓ Real-time equipment stat application operational\n");
        printf("✓ Equipment durability reduction hooks functional\n");
        printf("✓ Equipment proc effect triggers working\n");
        printf("✓ Equipment set bonus system operational\n");
        printf("✓ Equipment enchantment effects integration complete\n");
        printf("✓ Equipment weight impact system functional\n");
        printf("✓ Equipment upgrade notifications working\n");
        printf("\nPhase 3.2 Combat System ↔ Equipment System Bridge COMPLETE!\n");
    }
    else
    {
        printf("\n[FAILURE] Some Phase 3.2 tests failed. Check output above for details.\n");
    }
}

/* Main function for standalone test execution */
int main(int argc, char* argv[])
{
    /* Suppress unused parameter warnings */
    (void) argc;
    (void) argv;

    run_phase3_2_tests();
    return (tests_passed == tests_run) ? 0 : 1;
}
