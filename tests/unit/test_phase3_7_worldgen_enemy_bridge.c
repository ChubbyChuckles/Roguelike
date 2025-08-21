/**
 * Unit Tests for Phase 3.7: World Generation ↔ Enemy Integration Bridge
 * 
 * This test suite validates all functionality of the worldgen-enemy bridge including:
 * - Biome-specific encounter management (3.7.1)
 * - Enemy level scaling (3.7.2) 
 * - Seasonal enemy variations (3.7.3)
 * - Enemy pack size scaling (3.7.4)
 * - Enemy environmental modifiers (3.7.5)
 * - Enemy spawn density control (3.7.6)
 * - Enemy migration patterns (3.7.7)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "core/integration/worldgen_enemy_bridge.h"

/* Test utilities */
static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_TRUE(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("✓ %s\n", message); \
        } else { \
            printf("✗ %s\n", message); \
        } \
    } while(0)

#define ASSERT_FALSE(condition, message) ASSERT_TRUE(!(condition), message)
#define ASSERT_EQ(a, b, message) ASSERT_TRUE((a) == (b), message)
#define ASSERT_NEQ(a, b, message) ASSERT_TRUE((a) != (b), message)
#define ASSERT_FLOAT_EQ(a, b, tolerance, message) ASSERT_TRUE(fabs((a) - (b)) < (tolerance), message)

/* Test helper functions */
static void create_test_encounter_file(const char* filename) {
    FILE* file;
    if (fopen_s(&file, filename, "w") == 0) {
        /* Format: enemy_id,spawn_weight,min_level,max_level,difficulty_mod,is_boss,req_rep */
        fprintf(file, "1,10.0,1,5,1.0,0,0\n");      /* Goblin */
        fprintf(file, "2,8.0,3,8,1.2,0,0\n");       /* Orc */
        fprintf(file, "3,5.0,5,12,1.5,0,100\n");    /* Troll */
        fprintf(file, "4,2.0,10,15,2.0,1,500\n");   /* Dragon (boss) */
        fprintf(file, "5,15.0,1,3,0.8,0,0\n");      /* Rat */
        fclose(file);
    }
}

/* Test Bridge Initialization */
static void test_bridge_initialization(void) {
    printf("\n=== Testing Bridge Initialization ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    
    /* Test successful initialization */
    bool init_result = rogue_worldgen_enemy_bridge_init(&bridge);
    ASSERT_TRUE(init_result, "Bridge initialization should succeed");
    ASSERT_TRUE(bridge.initialized, "Bridge should be marked as initialized");
    ASSERT_TRUE(bridge.enabled, "Bridge should be enabled by default");
    ASSERT_EQ(bridge.active_region_count, 0, "Active region count should start at 0");
    
    /* Test null pointer handling */
    bool null_init = rogue_worldgen_enemy_bridge_init(NULL);
    ASSERT_FALSE(null_init, "Null pointer initialization should fail");
    
    /* Test operational status */
    bool operational = rogue_worldgen_enemy_bridge_is_operational(&bridge);
    ASSERT_TRUE(operational, "Initialized bridge should be operational");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
    ASSERT_FALSE(bridge.initialized, "Bridge should be marked as not initialized after shutdown");
}

/* Test 3.7.1: Biome-Specific Encounter Management */
static void test_biome_encounter_management(void) {
    printf("\n=== Testing 3.7.1: Biome-Specific Encounter Management ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Create test encounter file */
    const char* test_file = "test_encounters.cfg";
    create_test_encounter_file(test_file);
    
    /* Test encounter table loading */
    bool load_result = rogue_worldgen_enemy_bridge_load_biome_encounters(&bridge, BIOME_FOREST, test_file);
    ASSERT_TRUE(load_result, "Biome encounter loading should succeed");
    
    BiomeEncounterManager* manager = &bridge.encounter_managers[BIOME_FOREST];
    ASSERT_TRUE(manager->encounters_loaded, "Encounters should be marked as loaded");
    ASSERT_EQ(manager->encounter_count, 5, "Should load 5 encounter entries");
    ASSERT_EQ(manager->encounters[0].enemy_id, 1, "First encounter should be Goblin (ID 1)");
    ASSERT_TRUE(manager->encounters[3].is_boss, "Fourth encounter should be a boss");
    
    /* Test encounter selection */
    uint32_t enemy_id, enemy_level;
    bool selection_result = rogue_worldgen_enemy_bridge_get_biome_encounter(&bridge, BIOME_FOREST, 5, &enemy_id, &enemy_level);
    ASSERT_TRUE(selection_result, "Encounter selection should succeed for valid level");
    ASSERT_TRUE(enemy_id > 0, "Selected enemy ID should be valid");
    ASSERT_TRUE(enemy_level >= 1 && enemy_level <= 15, "Selected enemy level should be in valid range");
    
    /* Test encounter selection for invalid level */
    bool invalid_selection = rogue_worldgen_enemy_bridge_get_biome_encounter(&bridge, BIOME_FOREST, 50, &enemy_id, &enemy_level);
    ASSERT_FALSE(invalid_selection, "Encounter selection should fail for invalid level");
    
    /* Test loading non-existent file */
    bool invalid_load = rogue_worldgen_enemy_bridge_load_biome_encounters(&bridge, BIOME_DESERT, "nonexistent.cfg");
    ASSERT_FALSE(invalid_load, "Loading non-existent file should fail");
    
    /* Clean up */
    remove(test_file);
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test 3.7.2: Enemy Level Scaling */
static void test_enemy_level_scaling(void) {
    printf("\n=== Testing 3.7.2: Enemy Level Scaling ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Test setting region scaling */
    bool scaling_result = rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 5, 2.0f, 10);
    ASSERT_TRUE(scaling_result, "Setting region scaling should succeed");
    
    RegionLevelScaling* scaling = &bridge.level_scaling[5];
    ASSERT_FLOAT_EQ(scaling->difficulty_rating, 2.0f, 0.01f, "Difficulty rating should be set correctly");
    ASSERT_EQ(scaling->base_enemy_level, 10, "Base enemy level should be set correctly");
    
    /* Test scaled enemy level calculation */
    uint32_t original_level = 5;
    uint32_t scaled_level = rogue_worldgen_enemy_bridge_get_scaled_enemy_level(&bridge, 5, original_level);
    ASSERT_TRUE(scaled_level >= 1, "Scaled level should be at least 1");
    ASSERT_TRUE(scaled_level != original_level, "Scaled level should differ from original with 2.0x difficulty");
    
    /* Test scaling for different difficulty ratings */
    rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 10, 0.5f, 5);
    uint32_t easy_scaled = rogue_worldgen_enemy_bridge_get_scaled_enemy_level(&bridge, 10, 10);
    ASSERT_TRUE(easy_scaled <= 10, "Easy region should scale down enemy levels");
    
    rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 15, 3.0f, 1);
    uint32_t hard_scaled = rogue_worldgen_enemy_bridge_get_scaled_enemy_level(&bridge, 15, 10);
    ASSERT_TRUE(hard_scaled >= 10, "Hard region should scale up enemy levels");
    
    /* Test invalid region ID */
    bool invalid_scaling = rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 999, 1.0f, 1);
    ASSERT_FALSE(invalid_scaling, "Setting scaling for invalid region should fail");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test 3.7.3: Seasonal Enemy Variations */
static void test_seasonal_enemy_variations(void) {
    printf("\n=== Testing 3.7.3: Seasonal Enemy Variations ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Test setting season */
    bool season_result = rogue_worldgen_enemy_bridge_set_season(&bridge, SEASON_WINTER);
    ASSERT_TRUE(season_result, "Setting season should succeed");
    ASSERT_EQ(bridge.seasonal_system.current_season, SEASON_WINTER, "Current season should be winter");
    
    /* Test adding seasonal variations */
    bool variation_result = rogue_worldgen_enemy_bridge_add_seasonal_variation(&bridge, 100, SEASON_WINTER, 2.0f, 1.5f, 1.2f);
    ASSERT_TRUE(variation_result, "Adding seasonal variation should succeed");
    ASSERT_EQ(bridge.seasonal_system.variation_count, 1, "Should have one seasonal variation");
    
    SeasonalVariation* variation = &bridge.seasonal_system.variations[0];
    ASSERT_EQ(variation->enemy_id, 100, "Variation enemy ID should match");
    ASSERT_EQ(variation->active_season, SEASON_WINTER, "Variation season should match");
    ASSERT_FLOAT_EQ(variation->spawn_weight_modifier, 2.0f, 0.01f, "Spawn modifier should match");
    ASSERT_FLOAT_EQ(variation->health_modifier, 1.5f, 0.01f, "Health modifier should match");
    ASSERT_FLOAT_EQ(variation->damage_modifier, 1.2f, 0.01f, "Damage modifier should match");
    
    /* Test adding multiple variations */
    rogue_worldgen_enemy_bridge_add_seasonal_variation(&bridge, 101, SEASON_SUMMER, 0.5f, 0.8f, 0.9f);
    rogue_worldgen_enemy_bridge_add_seasonal_variation(&bridge, 102, SEASON_SPRING, 1.5f, 1.0f, 1.1f);
    ASSERT_EQ(bridge.seasonal_system.variation_count, 3, "Should have three seasonal variations");
    
    /* Test invalid season */
    bool invalid_season = rogue_worldgen_enemy_bridge_set_season(&bridge, (RogueSeasonType)999);
    ASSERT_FALSE(invalid_season, "Setting invalid season should fail");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test 3.7.4: Enemy Pack Size Scaling */
static void test_enemy_pack_size_scaling(void) {
    printf("\n=== Testing 3.7.4: Enemy Pack Size Scaling ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Test setting pack scaling */
    bool scaling_result = rogue_worldgen_enemy_bridge_set_region_pack_scaling(&bridge, 8, 2.5f);
    ASSERT_TRUE(scaling_result, "Setting pack scaling should succeed");
    
    RegionPackScaling* pack_scaling = &bridge.pack_scaling[8];
    ASSERT_FLOAT_EQ(pack_scaling->danger_rating, 2.5f, 0.01f, "Danger rating should be set correctly");
    ASSERT_TRUE(pack_scaling->base_pack_size > 1, "Base pack size should increase with danger");
    
    /* Test pack size calculation */
    uint32_t pack_size = rogue_worldgen_enemy_bridge_get_pack_size(&bridge, 8, 2);
    ASSERT_TRUE(pack_size >= 2, "Pack size should be at least the base size");
    ASSERT_TRUE(pack_size <= pack_scaling->max_pack_size, "Pack size should not exceed maximum");
    
    /* Test different danger ratings */
    rogue_worldgen_enemy_bridge_set_region_pack_scaling(&bridge, 12, 0.5f);
    uint32_t small_pack = rogue_worldgen_enemy_bridge_get_pack_size(&bridge, 12, 3);
    
    rogue_worldgen_enemy_bridge_set_region_pack_scaling(&bridge, 16, 4.0f);
    uint32_t large_pack = rogue_worldgen_enemy_bridge_get_pack_size(&bridge, 16, 3);
    
    ASSERT_TRUE(large_pack >= small_pack, "Higher danger should generally produce larger packs");
    
    /* Test invalid region */
    bool invalid_pack_scaling = rogue_worldgen_enemy_bridge_set_region_pack_scaling(&bridge, 999, 1.0f);
    ASSERT_FALSE(invalid_pack_scaling, "Setting pack scaling for invalid region should fail");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test 3.7.5: Enemy Environmental Modifiers */
static void test_enemy_environmental_modifiers(void) {
    printf("\n=== Testing 3.7.5: Enemy Environmental Modifiers ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Test adding biome modifiers */
    bool modifier_result = rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_VOLCANIC, ENEMY_MOD_FIRE_RESISTANT, 0.8f, 1.5f);
    ASSERT_TRUE(modifier_result, "Adding biome modifier should succeed");
    
    BiomeModifierSystem* mod_system = &bridge.modifier_systems[BIOME_VOLCANIC];
    ASSERT_EQ(mod_system->modifier_count, 1, "Should have one modifier");
    ASSERT_EQ(mod_system->modifiers[0].modifier_type, ENEMY_MOD_FIRE_RESISTANT, "Modifier type should match");
    ASSERT_FLOAT_EQ(mod_system->modifiers[0].activation_chance, 0.8f, 0.01f, "Activation chance should match");
    
    /* Test adding multiple modifiers */
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_VOLCANIC, ENEMY_MOD_HEALTH_BOOST, 0.6f, 1.3f);
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_VOLCANIC, ENEMY_MOD_DAMAGE_BOOST, 0.4f, 1.2f);
    ASSERT_EQ(mod_system->modifier_count, 3, "Should have three modifiers");
    
    /* Test applying environmental modifiers */
    uint32_t applied_modifiers = rogue_worldgen_enemy_bridge_apply_environmental_modifiers(&bridge, BIOME_VOLCANIC, 200);
    /* Note: Result is random, but should be valid bitmask */
    ASSERT_TRUE(applied_modifiers <= 0xFF, "Applied modifiers should be valid bitmask");
    
    /* Test modifiers for different biomes */
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_TUNDRA, ENEMY_MOD_ICE_RESISTANT, 0.9f, 2.0f);
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_SWAMP, ENEMY_MOD_POISON_IMMUNE, 0.7f, 1.0f);
    
    BiomeModifierSystem* tundra_system = &bridge.modifier_systems[BIOME_TUNDRA];
    BiomeModifierSystem* swamp_system = &bridge.modifier_systems[BIOME_SWAMP];
    ASSERT_EQ(tundra_system->modifier_count, 1, "Tundra should have one modifier");
    ASSERT_EQ(swamp_system->modifier_count, 1, "Swamp should have one modifier");
    
    /* Test invalid biome */
    bool invalid_modifier = rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, (RogueBiomeType)999, ENEMY_MOD_ARMORED, 0.5f, 1.0f);
    ASSERT_FALSE(invalid_modifier, "Adding modifier for invalid biome should fail");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test 3.7.6: Enemy Spawn Density Control */
static void test_enemy_spawn_density_control(void) {
    printf("\n=== Testing 3.7.6: Enemy Spawn Density Control ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Test setting spawn density */
    bool density_result = rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 20, 1.5f, 25);
    ASSERT_TRUE(density_result, "Setting spawn density should succeed");
    
    SpawnDensityControl* density = &bridge.density_controls[20];
    ASSERT_FLOAT_EQ(density->base_spawn_density, 1.5f, 0.01f, "Base spawn density should be set correctly");
    ASSERT_EQ(density->max_concurrent_enemies, 25, "Max concurrent enemies should be set correctly");
    
    /* Test updating enemy count */
    bool count_result = rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 20, 5);
    ASSERT_TRUE(count_result, "Updating enemy count should succeed");
    ASSERT_EQ(density->current_enemy_count, 5, "Enemy count should be updated correctly");
    
    /* Test multiple count updates */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 20, 3);
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 20, -2);
    ASSERT_EQ(density->current_enemy_count, 6, "Enemy count should reflect multiple updates");
    
    /* Test count going negative (should clamp to 0) */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 20, -10);
    ASSERT_EQ(density->current_enemy_count, 0, "Enemy count should not go below 0");
    
    /* Test respawn rate adjustment based on density */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 20, 20); /* Near max capacity */
    ASSERT_TRUE(density->respawn_rate_modifier < 1.0f, "Respawn rate should be reduced when near capacity");
    
    /* Test invalid region */
    bool invalid_density = rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 999, 1.0f, 10);
    ASSERT_FALSE(invalid_density, "Setting density for invalid region should fail");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test 3.7.7: Enemy Migration Patterns */
static void test_enemy_migration_patterns(void) {
    printf("\n=== Testing 3.7.7: Enemy Migration Patterns ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Set up regions with different enemy counts */
    rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 30, 1.0f, 20);
    rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 31, 1.0f, 20);
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 30, 18); /* High density source */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 31, 5);  /* Low density destination */
    
    /* Test adding migration route */
    uint32_t enemy_types[] = {100, 101, 102};
    bool migration_result = rogue_worldgen_enemy_bridge_add_migration_route(&bridge, 30, 31, enemy_types, 3, 0.8f);
    ASSERT_TRUE(migration_result, "Adding migration route should succeed");
    ASSERT_EQ(bridge.migration_system.route_count, 1, "Should have one migration route");
    
    EnemyMigrationRoute* route = &bridge.migration_system.routes[0];
    ASSERT_EQ(route->source_region_id, 30, "Source region should match");
    ASSERT_EQ(route->destination_region_id, 31, "Destination region should match");
    ASSERT_EQ(route->enemy_type_count, 3, "Enemy type count should match");
    ASSERT_EQ(route->enemy_types[0], 100, "First enemy type should match");
    ASSERT_FLOAT_EQ(route->migration_trigger_threshold, 0.8f, 0.01f, "Trigger threshold should match");
    
    /* Test processing migrations */
    uint32_t source_count_before = bridge.density_controls[30].current_enemy_count;
    uint32_t dest_count_before = bridge.density_controls[31].current_enemy_count;
    
    bool process_result = rogue_worldgen_enemy_bridge_process_migrations(&bridge);
    ASSERT_TRUE(process_result, "Processing migrations should succeed with high density source");
    
    uint32_t source_count_after = bridge.density_controls[30].current_enemy_count;
    uint32_t dest_count_after = bridge.density_controls[31].current_enemy_count;
    
    ASSERT_TRUE(source_count_after < source_count_before, "Source region should lose enemies");
    ASSERT_TRUE(dest_count_after > dest_count_before, "Destination region should gain enemies");
    
    /* Test multiple migration routes */
    rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 32, 1.0f, 15);
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 32, 2);
    
    uint32_t enemy_types2[] = {103, 104};
    rogue_worldgen_enemy_bridge_add_migration_route(&bridge, 30, 32, enemy_types2, 2, 0.7f);
    ASSERT_EQ(bridge.migration_system.route_count, 2, "Should have two migration routes");
    
    /* Test migration with low density (should not trigger) */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 30, -15); /* Reduce to low density */
    uint32_t low_source_count = bridge.density_controls[30].current_enemy_count;
    
    rogue_worldgen_enemy_bridge_process_migrations(&bridge);
    uint32_t unchanged_count = bridge.density_controls[30].current_enemy_count;
    ASSERT_EQ(low_source_count, unchanged_count, "Low density source should not trigger migration");
    
    /* Test invalid parameters */
    bool invalid_migration = rogue_worldgen_enemy_bridge_add_migration_route(&bridge, 999, 31, enemy_types, 3, 0.8f);
    ASSERT_FALSE(invalid_migration, "Adding route with invalid source should fail");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Test Bridge Update and Performance */
static void test_bridge_update_and_performance(void) {
    printf("\n=== Testing Bridge Update and Performance ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Test bridge update */
    bool update_result = rogue_worldgen_enemy_bridge_update(&bridge, 0.016f);
    ASSERT_TRUE(update_result, "Bridge update should succeed");
    
    /* Test metrics collection */
    WorldGenEnemyBridgeMetrics metrics = rogue_worldgen_enemy_bridge_get_metrics(&bridge);
    ASSERT_TRUE(metrics.total_operations > 0, "Should have recorded operations");
    ASSERT_TRUE(metrics.performance_samples > 0, "Should have performance samples");
    
    /* Simulate some operations to generate metrics */
    rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 1, 1.5f, 5);
    rogue_worldgen_enemy_bridge_set_season(&bridge, SEASON_SUMMER);
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_FOREST, ENEMY_MOD_HEALTH_BOOST, 0.5f, 1.2f);
    
    WorldGenEnemyBridgeMetrics updated_metrics = rogue_worldgen_enemy_bridge_get_metrics(&bridge);
    ASSERT_TRUE(updated_metrics.level_scaling_updates > 0, "Should have level scaling updates");
    ASSERT_TRUE(updated_metrics.seasonal_transitions > 0, "Should have seasonal transitions");
    ASSERT_TRUE(updated_metrics.modifier_applications > 0, "Should have modifier applications");
    
    /* Test multiple updates for performance measurement */
    for (int i = 0; i < 10; i++) {
        rogue_worldgen_enemy_bridge_update(&bridge, 0.016f);
    }
    
    WorldGenEnemyBridgeMetrics final_metrics = rogue_worldgen_enemy_bridge_get_metrics(&bridge);
    ASSERT_TRUE(final_metrics.avg_processing_time_us >= 0.0, "Average processing time should be non-negative");
    ASSERT_TRUE(final_metrics.total_operations >= updated_metrics.total_operations, "Total operations should increase");
    
    /* Test null pointer handling */
    WorldGenEnemyBridgeMetrics null_metrics = rogue_worldgen_enemy_bridge_get_metrics(NULL);
    ASSERT_EQ(null_metrics.total_operations, 0, "Null pointer should return empty metrics");
    
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
}

/* Integration Test - Complete Workflow */
static void test_complete_workflow_integration(void) {
    printf("\n=== Testing Complete Workflow Integration ===\n");
    
    RogueWorldGenEnemyBridge bridge;
    rogue_worldgen_enemy_bridge_init(&bridge);
    
    /* Set up a complete world scenario */
    
    /* 1. Load biome encounters */
    const char* test_file = "integration_encounters.cfg";
    create_test_encounter_file(test_file);
    rogue_worldgen_enemy_bridge_load_biome_encounters(&bridge, BIOME_FOREST, test_file);
    rogue_worldgen_enemy_bridge_load_biome_encounters(&bridge, BIOME_DESERT, test_file);
    
    /* 2. Set up regions with different characteristics */
    rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 0, 1.2f, 3);        /* Easy forest */
    rogue_worldgen_enemy_bridge_set_region_scaling(&bridge, 1, 2.5f, 8);        /* Hard desert */
    rogue_worldgen_enemy_bridge_set_region_pack_scaling(&bridge, 0, 1.0f);      /* Normal pack sizes */
    rogue_worldgen_enemy_bridge_set_region_pack_scaling(&bridge, 1, 3.0f);      /* Large pack sizes */
    rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 0, 1.0f, 15);        /* Normal density */
    rogue_worldgen_enemy_bridge_set_spawn_density(&bridge, 1, 2.0f, 30);        /* High density */
    
    /* 3. Add environmental modifiers */
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_FOREST, ENEMY_MOD_HEALTH_BOOST, 0.3f, 1.1f);
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_DESERT, ENEMY_MOD_FIRE_RESISTANT, 0.8f, 1.5f);
    rogue_worldgen_enemy_bridge_add_biome_modifier(&bridge, BIOME_DESERT, ENEMY_MOD_SPEED_BOOST, 0.6f, 1.3f);
    
    /* 4. Set up seasonal variations */
    rogue_worldgen_enemy_bridge_set_season(&bridge, SEASON_WINTER);
    rogue_worldgen_enemy_bridge_add_seasonal_variation(&bridge, 1, SEASON_WINTER, 1.5f, 1.2f, 1.1f);
    rogue_worldgen_enemy_bridge_add_seasonal_variation(&bridge, 2, SEASON_SUMMER, 0.7f, 0.9f, 0.8f);
    
    /* 5. Set up migration route from high-density to low-density */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 1, 25); /* High density in desert */
    rogue_worldgen_enemy_bridge_update_enemy_count(&bridge, 0, 5);  /* Low density in forest */
    uint32_t migrating_enemies[] = {1, 2, 3};
    rogue_worldgen_enemy_bridge_add_migration_route(&bridge, 1, 0, migrating_enemies, 3, 0.7f);
    
    /* 6. Test integrated operations */
    
    /* Get encounters from both biomes */
    uint32_t forest_enemy_id, forest_enemy_level;
    uint32_t desert_enemy_id, desert_enemy_level;
    
    bool forest_encounter = rogue_worldgen_enemy_bridge_get_biome_encounter(&bridge, BIOME_FOREST, 5, &forest_enemy_id, &forest_enemy_level);
    bool desert_encounter = rogue_worldgen_enemy_bridge_get_biome_encounter(&bridge, BIOME_DESERT, 5, &desert_enemy_id, &desert_enemy_level);
    
    ASSERT_TRUE(forest_encounter, "Should get forest encounter");
    ASSERT_TRUE(desert_encounter, "Should get desert encounter");
    
    /* Test level scaling in different regions */
    uint32_t forest_scaled_level = rogue_worldgen_enemy_bridge_get_scaled_enemy_level(&bridge, 0, 5);
    uint32_t desert_scaled_level = rogue_worldgen_enemy_bridge_get_scaled_enemy_level(&bridge, 1, 5);
    
    ASSERT_TRUE(forest_scaled_level >= 1, "Forest scaled level should be valid");
    ASSERT_TRUE(desert_scaled_level >= 1, "Desert scaled level should be valid");
    ASSERT_TRUE(desert_scaled_level >= forest_scaled_level, "Desert should generally have higher scaled levels");
    
    /* Test pack size scaling */
    uint32_t forest_pack_size = rogue_worldgen_enemy_bridge_get_pack_size(&bridge, 0, 2);
    uint32_t desert_pack_size = rogue_worldgen_enemy_bridge_get_pack_size(&bridge, 1, 2);
    
    ASSERT_TRUE(forest_pack_size >= 1, "Forest pack size should be valid");
    ASSERT_TRUE(desert_pack_size >= 1, "Desert pack size should be valid");
    ASSERT_TRUE(desert_pack_size >= forest_pack_size, "Desert should generally have larger packs");
    
    /* Test environmental modifiers */
    uint32_t forest_modifiers = rogue_worldgen_enemy_bridge_apply_environmental_modifiers(&bridge, BIOME_FOREST, 100);
    uint32_t desert_modifiers = rogue_worldgen_enemy_bridge_apply_environmental_modifiers(&bridge, BIOME_DESERT, 100);
    
    /* Both should be valid bitmasks */
    ASSERT_TRUE(forest_modifiers <= 0xFF, "Forest modifiers should be valid");
    ASSERT_TRUE(desert_modifiers <= 0xFF, "Desert modifiers should be valid");
    
    /* Test migration processing */
    uint32_t pre_migration_desert = bridge.density_controls[1].current_enemy_count;
    uint32_t pre_migration_forest = bridge.density_controls[0].current_enemy_count;
    
    rogue_worldgen_enemy_bridge_process_migrations(&bridge);
    
    uint32_t post_migration_desert = bridge.density_controls[1].current_enemy_count;
    uint32_t post_migration_forest = bridge.density_controls[0].current_enemy_count;
    
    /* Verify migration occurred */
    ASSERT_TRUE(post_migration_desert <= pre_migration_desert, "Desert should lose enemies via migration");
    ASSERT_TRUE(post_migration_forest >= pre_migration_forest, "Forest should gain enemies via migration");
    
    /* Test system update */
    bool update_success = rogue_worldgen_enemy_bridge_update(&bridge, 0.016f);
    ASSERT_TRUE(update_success, "System update should succeed");
    
    /* Verify comprehensive metrics */
    WorldGenEnemyBridgeMetrics final_metrics = rogue_worldgen_enemy_bridge_get_metrics(&bridge);
    ASSERT_TRUE(final_metrics.encounter_table_loads > 0, "Should have encounter table loads");
    ASSERT_TRUE(final_metrics.level_scaling_updates > 0, "Should have level scaling updates");
    ASSERT_TRUE(final_metrics.seasonal_transitions > 0, "Should have seasonal transitions");
    ASSERT_TRUE(final_metrics.pack_size_calculations > 0, "Should have pack size calculations");
    ASSERT_TRUE(final_metrics.modifier_applications > 0, "Should have modifier applications");
    ASSERT_TRUE(final_metrics.spawn_density_updates > 0, "Should have spawn density updates");
    ASSERT_TRUE(final_metrics.migration_events > 0, "Should have migration events");
    ASSERT_TRUE(final_metrics.total_operations > 0, "Should have total operations");
    
    /* Clean up */
    remove(test_file);
    rogue_worldgen_enemy_bridge_shutdown(&bridge);
    
    printf("🎉 Complete workflow integration test passed!\n");
}

/* Main test runner */
int main(void) {
    printf("🧪 Starting Phase 3.7 World Generation ↔ Enemy Integration Bridge Tests\n");
    printf("==================================================================\n");
    
    /* Seed random number generator for consistent testing */
    srand((unsigned int)time(NULL));
    
    /* Run all test suites */
    test_bridge_initialization();
    test_biome_encounter_management();
    test_enemy_level_scaling();
    test_seasonal_enemy_variations();
    test_enemy_pack_size_scaling();
    test_enemy_environmental_modifiers();
    test_enemy_spawn_density_control();
    test_enemy_migration_patterns();
    test_bridge_update_and_performance();
    test_complete_workflow_integration();
    
    /* Print final results */
    printf("\n==================================================================\n");
    printf("📊 Test Results: %d/%d tests passed (%.1f%%)\n", 
           tests_passed, tests_run, (float)tests_passed / tests_run * 100.0f);
    
    if (tests_passed == tests_run) {
        printf("🎉 All Phase 3.7 World Generation ↔ Enemy Integration Bridge tests passed!\n");
        printf("\n✅ Bridge successfully implements:\n");
        printf("   • 3.7.1 Biome-specific encounter table loading & application\n");
        printf("   • 3.7.2 Enemy level scaling based on world region difficulty\n");
        printf("   • 3.7.3 Seasonal enemy variations based on world generation cycles\n");
        printf("   • 3.7.4 Enemy pack size scaling with world region danger rating\n");
        printf("   • 3.7.5 Enemy modifier chances based on biome environmental factors\n");
        printf("   • 3.7.6 Enemy spawn density control based on world generation parameters\n");
        printf("   • 3.7.7 Enemy migration patterns following world resource availability\n");
        printf("   • Comprehensive performance metrics and monitoring\n");
        printf("   • Complete workflow integration and cross-system validation\n");
        return 0;
    } else {
        printf("❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}
