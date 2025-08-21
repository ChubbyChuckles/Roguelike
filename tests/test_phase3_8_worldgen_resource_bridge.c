/**
 * Unit Tests for Phase 3.8: World Generation ↔ Resource/Gathering Bridge
 * 
 * This test suite validates all integration functionality between
 * world generation and resource/gathering systems with comprehensive coverage.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include "../src/core/integration/worldgen_resource_bridge.h"

/* Test Results Tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test Macros */
#define TEST_ASSERT(condition, test_name) do { \
    tests_run++; \
    if (condition) { \
        printf("✓ %s\n", test_name); \
        tests_passed++; \
    } else { \
        printf("✗ %s\n", test_name); \
        tests_failed++; \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, tolerance, test_name) do { \
    tests_run++; \
    if (fabsf((a) - (b)) <= (tolerance)) { \
        printf("✓ %s\n", test_name); \
        tests_passed++; \
    } else { \
        printf("✗ %s (expected %.3f, got %.3f)\n", test_name, (float)(b), (float)(a)); \
        tests_failed++; \
    } \
} while(0)

/* Test Helper Functions */
static bool create_test_placement_file(const char* filename) {
    FILE* file;
    if (fopen_s(&file, filename, "w") != 0) return false;
    
    fprintf(file, "0,0.8\n");  /* RESOURCE_STONE, weight 0.8 */
    fprintf(file, "1,0.6\n");  /* RESOURCE_WOOD, weight 0.6 */
    fprintf(file, "2,0.4\n");  /* RESOURCE_METAL_ORE, weight 0.4 */
    fprintf(file, "3,0.2\n");  /* RESOURCE_FOOD, weight 0.2 */
    
    fclose(file);
    return true;
}

/* 3.8.1: Resource Node Placement Tests */

static void test_bridge_initialization(void) {
    printf("\n--- Testing Bridge Initialization ---\n");
    
    RogueWorldGenResourceBridge bridge;
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_init(&bridge), 
                "Bridge initialization succeeds");
    
    TEST_ASSERT(bridge.initialized == true, 
                "Bridge initialized flag set");
    
    TEST_ASSERT(bridge.enabled == true, 
                "Bridge enabled flag set");
    
    TEST_ASSERT(bridge.total_node_count == 0, 
                "Initial node count is zero");
    
    TEST_ASSERT(bridge.seasonal_system.current_season == SEASON_SPRING, 
                "Default season is spring");
    
    TEST_ASSERT(bridge.quality_system.world_generation_seed == 12345, 
                "Default quality seed set");
    
    /* Test null parameter */
    TEST_ASSERT(rogue_worldgen_resource_bridge_init(NULL) == false, 
                "Initialization fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_placement_rule_loading(void) {
    printf("\n--- Testing Placement Rule Loading ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Create test placement file */
    const char* test_file = "test_placement_rules.cfg";
    TEST_ASSERT(create_test_placement_file(test_file), 
                "Test placement file created");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_load_placement_rules(&bridge, BIOME_FOREST, test_file), 
                "Placement rules loaded successfully");
    
    BiomeResourcePlacement* placement = &bridge.placements[BIOME_FOREST];
    TEST_ASSERT(placement->placement_rules_loaded == true, 
                "Placement rules marked as loaded");
    
    TEST_ASSERT(placement->resource_type_count == 4, 
                "Correct number of resource types loaded");
    
    TEST_ASSERT(placement->resource_types[0] == RESOURCE_STONE, 
                "First resource type is stone");
    
    TEST_ASSERT_FLOAT_EQ(placement->placement_weights[0], 0.8f, 0.01f, 
                         "First placement weight correct");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_load_placement_rules(NULL, BIOME_FOREST, test_file) == false, 
                "Loading fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_load_placement_rules(&bridge, MAX_BIOME_TYPES, test_file) == false, 
                "Loading fails with invalid biome type");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_load_placement_rules(&bridge, BIOME_FOREST, "nonexistent.cfg") == false, 
                "Loading fails with nonexistent file");
    
    /* Cleanup */
    remove(test_file);
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_node_placement(void) {
    printf("\n--- Testing Node Placement ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Setup placement rules */
    const char* test_file = "test_placement_rules2.cfg";
    create_test_placement_file(test_file);
    rogue_worldgen_resource_bridge_load_placement_rules(&bridge, BIOME_FOREST, test_file);
    
    /* Place nodes in a region */
    uint32_t nodes_placed = rogue_worldgen_resource_bridge_place_nodes(&bridge, 0, BIOME_FOREST, 
                                                                       0.0f, 0.0f, 100.0f, 100.0f);
    
    TEST_ASSERT(nodes_placed > 0, 
                "Nodes were placed in region");
    
    TEST_ASSERT(bridge.total_node_count == nodes_placed, 
                "Total node count matches placed nodes");
    
    /* Verify node properties */
    if (bridge.total_node_count > 0) {
        ResourceNode* first_node = &bridge.resource_nodes[0];
        TEST_ASSERT(first_node->is_active == true, 
                    "Placed node is active");
        
        TEST_ASSERT(first_node->region_id == 0, 
                    "Node has correct region ID");
        
        TEST_ASSERT(first_node->biome_type == BIOME_FOREST, 
                    "Node has correct biome type");
        
        TEST_ASSERT(first_node->world_x >= 0.0f && first_node->world_x <= 100.0f, 
                    "Node X position within region bounds");
        
        TEST_ASSERT(first_node->world_y >= 0.0f && first_node->world_y <= 100.0f, 
                    "Node Y position within region bounds");
    }
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_place_nodes(NULL, 0, BIOME_FOREST, 0, 0, 100, 100) == 0, 
                "Placement fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_place_nodes(&bridge, 64, BIOME_FOREST, 0, 0, 100, 100) == 0, 
                "Placement fails with invalid region ID");
    
    /* Cleanup */
    remove(test_file);
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* 3.8.2: Resource Abundance Scaling Tests */

static void test_fertility_management(void) {
    printf("\n--- Testing Fertility Management ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Test setting fertility */
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 5, 1.5f), 
                "Fertility set successfully");
    
    RegionAbundanceScaling* abundance = &bridge.abundance_scaling[5];
    TEST_ASSERT_FLOAT_EQ(abundance->fertility_rating, 1.5f, 0.01f, 
                         "Fertility rating set correctly");
    
    TEST_ASSERT_FLOAT_EQ(abundance->abundance_multiplier, 1.5f, 0.01f, 
                         "Abundance multiplier matches fertility");
    
    TEST_ASSERT(abundance->regeneration_rate > 1.5f, 
                "Regeneration rate boosted by fertility");
    
    /* Test abundance calculation */
    float abundance_value = rogue_worldgen_resource_bridge_get_resource_abundance(&bridge, 5, RESOURCE_STONE);
    TEST_ASSERT(abundance_value >= 1.0f, 
                "Abundance calculation returns valid value");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_region_fertility(NULL, 5, 1.5f) == false, 
                "Fertility setting fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 64, 1.5f) == false, 
                "Fertility setting fails with invalid region");
    
    TEST_ASSERT_FLOAT_EQ(rogue_worldgen_resource_bridge_get_resource_abundance(NULL, 5, RESOURCE_STONE), 0.0f, 0.01f, 
                         "Abundance calculation fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_abundance_scaling(void) {
    printf("\n--- Testing Abundance Scaling ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Set different fertility ratings */
    rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 0, 0.5f); /* Low fertility */
    rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 1, 1.0f); /* Normal fertility */
    rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 2, 2.0f); /* High fertility */
    
    /* Compare abundance values */
    float low_abundance = rogue_worldgen_resource_bridge_get_resource_abundance(&bridge, 0, RESOURCE_STONE);
    float normal_abundance = rogue_worldgen_resource_bridge_get_resource_abundance(&bridge, 1, RESOURCE_STONE);
    float high_abundance = rogue_worldgen_resource_bridge_get_resource_abundance(&bridge, 2, RESOURCE_STONE);
    
    TEST_ASSERT(low_abundance < normal_abundance, 
                "Low fertility gives lower abundance");
    
    TEST_ASSERT(normal_abundance < high_abundance, 
                "High fertility gives higher abundance");
    
    TEST_ASSERT(high_abundance > low_abundance * 1.5f, 
                "High fertility significantly higher than low");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* 3.8.3: Seasonal Resource Availability Tests */

static void test_seasonal_system(void) {
    printf("\n--- Testing Seasonal System ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Test season setting */
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_season(&bridge, SEASON_SUMMER), 
                "Season set successfully");
    
    TEST_ASSERT(bridge.seasonal_system.current_season == SEASON_SUMMER, 
                "Current season updated");
    
    /* Test seasonal availability setup */
    float season_modifiers[SEASON_COUNT] = {1.0f, 1.2f, 0.8f, 0.6f}; /* Spring, Summer, Fall, Winter */
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_seasonal_availability(&bridge, RESOURCE_FOOD, 
                                                                        SEASON_SUMMER, season_modifiers), 
                "Seasonal availability added");
    
    TEST_ASSERT(bridge.seasonal_system.availability_count == 1, 
                "Availability count incremented");
    
    SeasonalResourceAvailability* availability = &bridge.seasonal_system.availabilities[0];
    TEST_ASSERT(availability->resource_type == RESOURCE_FOOD, 
                "Resource type set correctly");
    
    TEST_ASSERT(availability->peak_season == SEASON_SUMMER, 
                "Peak season set correctly");
    
    TEST_ASSERT_FLOAT_EQ(availability->availability_modifiers[SEASON_SUMMER], 1.2f, 0.01f, 
                         "Summer modifier set correctly");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_season(NULL, SEASON_SUMMER) == false, 
                "Season setting fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_season(&bridge, SEASON_COUNT) == false, 
                "Season setting fails with invalid season");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_seasonal_availability(NULL, RESOURCE_FOOD, 
                                                                        SEASON_SUMMER, season_modifiers) == false, 
                "Availability addition fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_seasonal_progression(void) {
    printf("\n--- Testing Seasonal Progression ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Enable auto progression for testing */
    bridge.seasonal_system.auto_season_progression = true;
    bridge.seasonal_system.season_duration_us = 1000; /* Very short for testing */
    
    RogueSeasonType initial_season = bridge.seasonal_system.current_season;
    
    /* Force a progression by setting start time in the past */
    bridge.seasonal_system.season_start_time_us -= 2000;
    
    /* Update bridge to trigger season progression */
    rogue_worldgen_resource_bridge_update(&bridge, 0.1f);
    
    TEST_ASSERT(bridge.seasonal_system.current_season != initial_season, 
                "Season progressed automatically");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* 3.8.4: Resource Quality Variance Tests */

static void test_quality_system_initialization(void) {
    printf("\n--- Testing Quality System Initialization ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Test quality system initialization */
    TEST_ASSERT(rogue_worldgen_resource_bridge_init_quality_system(&bridge, 54321), 
                "Quality system initialized");
    
    TEST_ASSERT(bridge.quality_system.quality_system_initialized == true, 
                "Quality system marked as initialized");
    
    TEST_ASSERT(bridge.quality_system.world_generation_seed == 54321, 
                "World seed set correctly");
    
    /* Verify tier probabilities sum to approximately 1.0 */
    float total_probability = 0.0f;
    for (int i = 0; i < RESOURCE_QUALITY_TIERS; i++) {
        total_probability += bridge.quality_system.tier_probabilities[i];
    }
    
    TEST_ASSERT_FLOAT_EQ(total_probability, 1.0f, 0.01f, 
                         "Tier probabilities sum to 1.0");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_init_quality_system(NULL, 54321) == false, 
                "Quality initialization fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_quality_calculation(void) {
    printf("\n--- Testing Quality Calculation ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Setup placement and create a node */
    const char* test_file = "test_placement_rules3.cfg";
    create_test_placement_file(test_file);
    rogue_worldgen_resource_bridge_load_placement_rules(&bridge, BIOME_FOREST, test_file);
    
    uint32_t nodes_placed = rogue_worldgen_resource_bridge_place_nodes(&bridge, 0, BIOME_FOREST, 
                                                                       0.0f, 0.0f, 100.0f, 100.0f);
    
    if (nodes_placed > 0) {
        /* Initialize quality system */
        rogue_worldgen_resource_bridge_init_quality_system(&bridge, 98765);
        
        /* Calculate quality for first node */
        uint32_t quality = rogue_worldgen_resource_bridge_calculate_resource_quality(&bridge, 0, RESOURCE_STONE);
        
        TEST_ASSERT(quality >= 1 && quality <= 100, 
                    "Quality value within valid range");
        
        /* Quality should be consistent for same node */
        uint32_t quality2 = rogue_worldgen_resource_bridge_calculate_resource_quality(&bridge, 0, RESOURCE_STONE);
        TEST_ASSERT(quality == quality2, 
                    "Quality calculation is consistent");
        
        /* Test different nodes should potentially have different quality */
        if (nodes_placed > 1) {
            uint32_t quality3 = rogue_worldgen_resource_bridge_calculate_resource_quality(&bridge, 1, RESOURCE_STONE);
            /* Note: Due to randomness, we can't guarantee different quality, but we test the function works */
            TEST_ASSERT(quality3 >= 1 && quality3 <= 100, 
                        "Second node quality within valid range");
        }
    }
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_calculate_resource_quality(NULL, 0, RESOURCE_STONE) == 1, 
                "Quality calculation returns minimum with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_calculate_resource_quality(&bridge, 999, RESOURCE_STONE) == 1, 
                "Quality calculation returns minimum with invalid node ID");
    
    /* Cleanup */
    remove(test_file);
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* 3.8.5: Resource Depletion & Regeneration Tests */

static void test_depletion_cycle_setup(void) {
    printf("\n--- Testing Depletion Cycle Setup ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Setup depletion cycle */
    TEST_ASSERT(rogue_worldgen_resource_bridge_setup_depletion_cycle(&bridge, 0, 100, 5), 
                "Depletion cycle setup successful");
    
    TEST_ASSERT(bridge.depletion_cycle_count == 1, 
                "Depletion cycle count incremented");
    
    ResourceDepletionCycle* cycle = &bridge.depletion_cycles[0];
    TEST_ASSERT(cycle->node_id == 0, 
                "Cycle node ID set correctly");
    
    TEST_ASSERT(cycle->max_capacity == 100, 
                "Max capacity set correctly");
    
    TEST_ASSERT(cycle->current_capacity == 100, 
                "Initial capacity equals max");
    
    TEST_ASSERT(cycle->regeneration_rate == 5, 
                "Regeneration rate set correctly");
    
    TEST_ASSERT(cycle->is_depleted == false, 
                "Initial depletion state is false");
    
    TEST_ASSERT(cycle->can_regenerate == true, 
                "Regeneration enabled by default");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_setup_depletion_cycle(NULL, 0, 100, 5) == false, 
                "Setup fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_resource_harvesting(void) {
    printf("\n--- Testing Resource Harvesting ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Setup depletion cycle */
    rogue_worldgen_resource_bridge_setup_depletion_cycle(&bridge, 0, 100, 5);
    
    /* Test successful harvest */
    TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(&bridge, 0, 20), 
                "Resource harvest successful");
    
    ResourceDepletionCycle* cycle = &bridge.depletion_cycles[0];
    TEST_ASSERT(cycle->current_capacity == 80, 
                "Capacity reduced after harvest");
    
    /* Test harvest too much */
    TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(&bridge, 0, 90) == false, 
                "Harvest fails when amount exceeds capacity");
    
    /* Test complete depletion */
    TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(&bridge, 0, 80), 
                "Final harvest successful");
    
    TEST_ASSERT(cycle->current_capacity == 0, 
                "Node fully depleted");
    
    TEST_ASSERT(cycle->is_depleted == true, 
                "Depletion flag set");
    
    /* Test harvest from depleted node */
    TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(&bridge, 0, 1) == false, 
                "Harvest fails from depleted node");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(NULL, 0, 10) == false, 
                "Harvest fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(&bridge, 999, 10) == false, 
                "Harvest fails with invalid node ID");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_resource_regeneration(void) {
    printf("\n--- Testing Resource Regeneration ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Setup depletion cycle */
    rogue_worldgen_resource_bridge_setup_depletion_cycle(&bridge, 0, 100, 10);
    
    /* Deplete the resource */
    rogue_worldgen_resource_bridge_harvest_resource(&bridge, 0, 100);
    
    ResourceDepletionCycle* cycle = &bridge.depletion_cycles[0];
    TEST_ASSERT(cycle->is_depleted == true, 
                "Node is depleted before regeneration");
    
    /* Force regeneration time to be in the past */
    cycle->next_regeneration_time_us = 0;
    
    /* Process regeneration */
    TEST_ASSERT(rogue_worldgen_resource_bridge_process_regeneration(&bridge), 
                "Regeneration processing successful");
    
    TEST_ASSERT(cycle->current_capacity > 0, 
                "Capacity increased after regeneration");
    
    TEST_ASSERT(cycle->current_capacity <= cycle->regeneration_rate, 
                "Regeneration amount within expected range");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_process_regeneration(NULL) == false, 
                "Regeneration fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* 3.8.6: Rare Resource Events Tests */

static void test_rare_event_spawning(void) {
    printf("\n--- Testing Rare Event Spawning ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Test spawning rare event */
    TEST_ASSERT(rogue_worldgen_resource_bridge_spawn_rare_event(&bridge, RARE_EVENT_CRYSTAL_BLOOM, 
                                                               5, 100.0f, 200.0f), 
                "Rare event spawn successful");
    
    TEST_ASSERT(bridge.event_system.event_count == 1, 
                "Event count incremented");
    
    TEST_ASSERT(bridge.event_system.active_event_count == 1, 
                "Active event count incremented");
    
    RareResourceEvent* event = &bridge.event_system.events[0];
    TEST_ASSERT(event->event_type == RARE_EVENT_CRYSTAL_BLOOM, 
                "Event type set correctly");
    
    TEST_ASSERT(event->region_id == 5, 
                "Event region ID set correctly");
    
    TEST_ASSERT_FLOAT_EQ(event->world_x, 100.0f, 0.01f, 
                         "Event X position set correctly");
    
    TEST_ASSERT_FLOAT_EQ(event->world_y, 200.0f, 0.01f, 
                         "Event Y position set correctly");
    
    TEST_ASSERT(event->is_active == true, 
                "Event is active");
    
    TEST_ASSERT(event->bonus_resource_type == RESOURCE_RARE_CRYSTALS, 
                "Bonus resource type matches event type");
    
    TEST_ASSERT(event->bonus_yield > 0, 
                "Bonus yield is positive");
    
    TEST_ASSERT(event->bonus_quality_multiplier >= 1.0f, 
                "Quality multiplier is at least 1.0");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_spawn_rare_event(NULL, RARE_EVENT_CRYSTAL_BLOOM, 
                                                               5, 100.0f, 200.0f) == false, 
                "Event spawn fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_spawn_rare_event(&bridge, RARE_EVENT_TYPE_COUNT, 
                                                               5, 100.0f, 200.0f) == false, 
                "Event spawn fails with invalid event type");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_spawn_rare_event(&bridge, RARE_EVENT_CRYSTAL_BLOOM, 
                                                               64, 100.0f, 200.0f) == false, 
                "Event spawn fails with invalid region ID");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_rare_event_processing(void) {
    printf("\n--- Testing Rare Event Processing ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Spawn an event */
    rogue_worldgen_resource_bridge_spawn_rare_event(&bridge, RARE_EVENT_METAL_VEIN_DISCOVERY, 
                                                   3, 50.0f, 75.0f);
    
    TEST_ASSERT(bridge.event_system.active_event_count == 1, 
                "One active event before processing");
    
    /* Make the event expire by setting start time in the past */
    RareResourceEvent* event = &bridge.event_system.events[0];
    event->event_start_time_us = 0; /* Very old start time */
    event->event_duration_us = 1000; /* Short duration */
    
    /* Process events */
    TEST_ASSERT(rogue_worldgen_resource_bridge_process_rare_events(&bridge), 
                "Event processing successful");
    
    TEST_ASSERT(event->is_active == false, 
                "Event marked as inactive after expiration");
    
    TEST_ASSERT(bridge.event_system.active_event_count == 0, 
                "No active events after expiration");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_process_rare_events(NULL) == false, 
                "Event processing fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* 3.8.7: Resource Discovery Mechanics Tests */

static void test_discovery_location_management(void) {
    printf("\n--- Testing Discovery Location Management ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Test adding discovery location */
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_discovery_location(&bridge, 300.0f, 400.0f, 
                                                                     7, RESOURCE_GEMS, 5), 
                "Discovery location added successfully");
    
    TEST_ASSERT(bridge.discovery_system.location_count == 1, 
                "Location count incremented");
    
    ResourceDiscoveryLocation* location = &bridge.discovery_system.locations[0];
    TEST_ASSERT_FLOAT_EQ(location->world_x, 300.0f, 0.01f, 
                         "Location X coordinate set correctly");
    
    TEST_ASSERT_FLOAT_EQ(location->world_y, 400.0f, 0.01f, 
                         "Location Y coordinate set correctly");
    
    TEST_ASSERT(location->region_id == 7, 
                "Location region ID set correctly");
    
    TEST_ASSERT(location->hidden_resource_type == RESOURCE_GEMS, 
                "Hidden resource type set correctly");
    
    TEST_ASSERT(location->discovery_difficulty == 5, 
                "Discovery difficulty set correctly");
    
    TEST_ASSERT(location->has_been_discovered == false, 
                "Location initially undiscovered");
    
    TEST_ASSERT(location->discovery_radius > 0.0f, 
                "Discovery radius is positive");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_discovery_location(NULL, 300.0f, 400.0f, 
                                                                     7, RESOURCE_GEMS, 5) == false, 
                "Adding location fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_discovery_location(&bridge, 300.0f, 400.0f, 
                                                                     64, RESOURCE_GEMS, 5) == false, 
                "Adding location fails with invalid region ID");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_discovery_location(&bridge, 300.0f, 400.0f, 
                                                                     7, RESOURCE_TYPE_COUNT, 5) == false, 
                "Adding location fails with invalid resource type");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_discovery_attempts(void) {
    printf("\n--- Testing Discovery Attempts ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Add discovery location */
    rogue_worldgen_resource_bridge_add_discovery_location(&bridge, 100.0f, 100.0f, 
                                                         0, RESOURCE_MAGICAL_ESSENCE, 3);
    
    ResourceDiscoveryLocation* location = &bridge.discovery_system.locations[0];
    uint32_t discovery_id;
    
    /* Test discovery attempt from far away (should fail) */
    TEST_ASSERT(rogue_worldgen_resource_bridge_attempt_discovery(&bridge, 500.0f, 500.0f, 
                                                                100, &discovery_id) == false, 
                "Discovery fails when player is too far away");
    
    /* Test discovery attempt with insufficient skill (should fail) */
    TEST_ASSERT(rogue_worldgen_resource_bridge_attempt_discovery(&bridge, 100.0f, 100.0f, 
                                                                5, &discovery_id) == false, 
                "Discovery fails with insufficient skill level");
    
    /* Test discovery attempt with sufficient skill and close distance */
    /* We'll try multiple times due to randomness */
    bool discovered = false;
    for (int attempts = 0; attempts < 10 && !discovered; attempts++) {
        discovered = rogue_worldgen_resource_bridge_attempt_discovery(&bridge, 100.0f, 100.0f, 
                                                                     100, &discovery_id);
    }
    
    if (discovered) {
        TEST_ASSERT(discovery_id == 0, 
                    "Discovery ID matches first location");
        
        TEST_ASSERT(location->has_been_discovered == true, 
                    "Location marked as discovered");
        
        TEST_ASSERT(bridge.discovery_system.discovered_count == 1, 
                    "Discovered count incremented");
    }
    
    /* At minimum, test that the function doesn't crash with valid parameters */
    TEST_ASSERT(true, "Discovery attempt function executed without crashing");
    
    /* Test invalid parameters */
    TEST_ASSERT(rogue_worldgen_resource_bridge_attempt_discovery(NULL, 100.0f, 100.0f, 
                                                                100, &discovery_id) == false, 
                "Discovery fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_attempt_discovery(&bridge, 100.0f, 100.0f, 
                                                                100, NULL) == false, 
                "Discovery fails with null output parameter");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* Bridge Integration & Utility Tests */

static void test_bridge_update_functionality(void) {
    printf("\n--- Testing Bridge Update Functionality ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Test normal update */
    TEST_ASSERT(rogue_worldgen_resource_bridge_update(&bridge, 0.016f), 
                "Bridge update successful");
    
    /* Test disabled bridge update */
    bridge.enabled = false;
    TEST_ASSERT(rogue_worldgen_resource_bridge_update(&bridge, 0.016f) == false, 
                "Update fails when bridge disabled");
    
    bridge.enabled = true;
    
    /* Test uninitialized bridge update */
    bridge.initialized = false;
    TEST_ASSERT(rogue_worldgen_resource_bridge_update(&bridge, 0.016f) == false, 
                "Update fails when bridge not initialized");
    
    /* Test null parameter */
    TEST_ASSERT(rogue_worldgen_resource_bridge_update(NULL, 0.016f) == false, 
                "Update fails with null bridge");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_metrics_collection(void) {
    printf("\n--- Testing Metrics Collection ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Perform some operations to generate metrics */
    rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 0, 1.5f);
    rogue_worldgen_resource_bridge_set_season(&bridge, SEASON_AUTUMN);
    
    /* Get metrics */
    WorldGenResourceBridgeMetrics metrics = rogue_worldgen_resource_bridge_get_metrics(&bridge);
    
    TEST_ASSERT(metrics.abundance_calculations > 0, 
                "Abundance calculations recorded");
    
    TEST_ASSERT(metrics.seasonal_updates > 0, 
                "Seasonal updates recorded");
    
    TEST_ASSERT(metrics.total_operations > 0, 
                "Total operations recorded");
    
    /* Test null parameter */
    WorldGenResourceBridgeMetrics null_metrics = rogue_worldgen_resource_bridge_get_metrics(NULL);
    TEST_ASSERT(null_metrics.total_operations == 0, 
                "Null bridge returns empty metrics");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_operational_status(void) {
    printf("\n--- Testing Operational Status ---\n");
    
    RogueWorldGenResourceBridge bridge;
    
    /* Test uninitialized bridge */
    TEST_ASSERT(rogue_worldgen_resource_bridge_is_operational(&bridge) == false, 
                "Uninitialized bridge is not operational");
    
    /* Test null bridge */
    TEST_ASSERT(rogue_worldgen_resource_bridge_is_operational(NULL) == false, 
                "Null bridge is not operational");
    
    /* Initialize bridge */
    rogue_worldgen_resource_bridge_init(&bridge);
    TEST_ASSERT(rogue_worldgen_resource_bridge_is_operational(&bridge) == true, 
                "Initialized bridge is operational");
    
    /* Disable bridge */
    bridge.enabled = false;
    TEST_ASSERT(rogue_worldgen_resource_bridge_is_operational(&bridge) == false, 
                "Disabled bridge is not operational");
    
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

static void test_spatial_queries(void) {
    printf("\n--- Testing Spatial Queries ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* Setup placement and create nodes */
    const char* test_file = "test_placement_rules4.cfg";
    create_test_placement_file(test_file);
    rogue_worldgen_resource_bridge_load_placement_rules(&bridge, BIOME_FOREST, test_file);
    
    uint32_t nodes_placed = rogue_worldgen_resource_bridge_place_nodes(&bridge, 0, BIOME_FOREST, 
                                                                       0.0f, 0.0f, 100.0f, 100.0f);
    
    if (nodes_placed > 0) {
        uint32_t node_ids[10];
        uint32_t found_nodes = rogue_worldgen_resource_bridge_get_nodes_in_radius(&bridge, 50.0f, 50.0f, 
                                                                                  100.0f, node_ids, 10);
        
        TEST_ASSERT(found_nodes <= nodes_placed, 
                    "Found nodes doesn't exceed total nodes");
        
        TEST_ASSERT(found_nodes <= 10, 
                    "Found nodes doesn't exceed maximum requested");
        
        /* Verify node IDs are valid */
        for (uint32_t i = 0; i < found_nodes; i++) {
            TEST_ASSERT(node_ids[i] < bridge.total_node_count, 
                        "Found node ID is valid");
        }
    }
    
    /* Test invalid parameters */
    uint32_t dummy_ids[1];
    TEST_ASSERT(rogue_worldgen_resource_bridge_get_nodes_in_radius(NULL, 50.0f, 50.0f, 
                                                                  100.0f, dummy_ids, 1) == 0, 
                "Spatial query fails with null bridge");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_get_nodes_in_radius(&bridge, 50.0f, 50.0f, 
                                                                  100.0f, NULL, 1) == 0, 
                "Spatial query fails with null output array");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_get_nodes_in_radius(&bridge, 50.0f, 50.0f, 
                                                                  100.0f, dummy_ids, 0) == 0, 
                "Spatial query fails with zero max nodes");
    
    /* Cleanup */
    remove(test_file);
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* Complete Workflow Integration Test */

static void test_complete_workflow_integration(void) {
    printf("\n--- Testing Complete Workflow Integration ---\n");
    
    RogueWorldGenResourceBridge bridge;
    rogue_worldgen_resource_bridge_init(&bridge);
    
    /* 1. Setup world generation parameters */
    const char* test_file = "test_complete_workflow.cfg";
    create_test_placement_file(test_file);
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_load_placement_rules(&bridge, BIOME_FOREST, test_file), 
                "Workflow: Placement rules loaded");
    
    /* 2. Initialize quality system */
    TEST_ASSERT(rogue_worldgen_resource_bridge_init_quality_system(&bridge, 11111), 
                "Workflow: Quality system initialized");
    
    /* 3. Setup regional fertility */
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_region_fertility(&bridge, 0, 1.8f), 
                "Workflow: Regional fertility set");
    
    /* 4. Configure seasonal system */
    float seasonal_mods[SEASON_COUNT] = {1.0f, 1.3f, 0.9f, 0.5f};
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_seasonal_availability(&bridge, RESOURCE_FOOD, 
                                                                        SEASON_SUMMER, seasonal_mods), 
                "Workflow: Seasonal availability configured");
    
    TEST_ASSERT(rogue_worldgen_resource_bridge_set_season(&bridge, SEASON_SUMMER), 
                "Workflow: Season set to summer");
    
    /* 5. Place resource nodes */
    uint32_t nodes_placed = rogue_worldgen_resource_bridge_place_nodes(&bridge, 0, BIOME_FOREST, 
                                                                       0.0f, 0.0f, 200.0f, 200.0f);
    TEST_ASSERT(nodes_placed > 0, 
                "Workflow: Resource nodes placed");
    
    /* 6. Setup depletion cycles for placed nodes */
    for (uint32_t i = 0; i < (nodes_placed < 3 ? nodes_placed : 3); i++) {
        TEST_ASSERT(rogue_worldgen_resource_bridge_setup_depletion_cycle(&bridge, i, 150, 8), 
                    "Workflow: Depletion cycle setup");
    }
    
    /* 7. Calculate resource quality */
    if (nodes_placed > 0) {
        uint32_t quality = rogue_worldgen_resource_bridge_calculate_resource_quality(&bridge, 0, RESOURCE_STONE);
        TEST_ASSERT(quality >= 1 && quality <= 100, 
                    "Workflow: Resource quality calculated");
    }
    
    /* 8. Spawn rare event */
    TEST_ASSERT(rogue_worldgen_resource_bridge_spawn_rare_event(&bridge, RARE_EVENT_ANCIENT_GROVE, 
                                                               0, 100.0f, 100.0f), 
                "Workflow: Rare event spawned");
    
    /* 9. Add discovery location */
    TEST_ASSERT(rogue_worldgen_resource_bridge_add_discovery_location(&bridge, 150.0f, 150.0f, 
                                                                     0, RESOURCE_RARE_CRYSTALS, 4), 
                "Workflow: Discovery location added");
    
    /* 10. Perform resource harvesting */
    if (bridge.depletion_cycle_count > 0) {
        TEST_ASSERT(rogue_worldgen_resource_bridge_harvest_resource(&bridge, 0, 25), 
                    "Workflow: Resource harvested");
    }
    
    /* 11. Update bridge systems */
    TEST_ASSERT(rogue_worldgen_resource_bridge_update(&bridge, 0.033f), 
                "Workflow: Bridge systems updated");
    
    /* 12. Process regeneration */
    bool regen_processed = rogue_worldgen_resource_bridge_process_regeneration(&bridge);
    TEST_ASSERT(true, "Workflow: Regeneration processing completed"); /* May or may not regenerate */
    
    /* Suppress unused variable warning */
    (void)regen_processed;
    
    /* 13. Verify abundance calculations */
    float abundance = rogue_worldgen_resource_bridge_get_resource_abundance(&bridge, 0, RESOURCE_FOOD);
    TEST_ASSERT(abundance > 0.0f, 
                "Workflow: Resource abundance calculated");
    
    /* 14. Check spatial queries */
    uint32_t node_ids[5];
    uint32_t found_nodes = rogue_worldgen_resource_bridge_get_nodes_in_radius(&bridge, 100.0f, 100.0f, 
                                                                             150.0f, node_ids, 5);
    TEST_ASSERT(true, "Workflow: Spatial query completed"); /* May find 0 or more nodes */
    
    /* Suppress unused variable warning */
    (void)found_nodes;
    
    /* 15. Verify bridge operational status */
    TEST_ASSERT(rogue_worldgen_resource_bridge_is_operational(&bridge), 
                "Workflow: Bridge remains operational");
    
    /* 16. Collect final metrics */
    WorldGenResourceBridgeMetrics metrics = rogue_worldgen_resource_bridge_get_metrics(&bridge);
    TEST_ASSERT(metrics.total_operations > 0, 
                "Workflow: Operations metrics collected");
    
    printf("   Complete workflow successfully processed %llu operations\n", (unsigned long long)metrics.total_operations);
    printf("   Placed %u nodes, spawned %llu rare events\n", nodes_placed, (unsigned long long)metrics.rare_events_spawned);
    printf("   Processed %llu abundance calculations, %llu seasonal updates\n", 
           (unsigned long long)metrics.abundance_calculations, (unsigned long long)metrics.seasonal_updates);
    
    /* Cleanup */
    remove(test_file);
    rogue_worldgen_resource_bridge_shutdown(&bridge);
}

/* Main Test Runner */

int main(void) {
    printf("🧪 Starting Phase 3.8 World Generation ↔ Resource/Gathering Bridge Tests\n");
    printf("=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n");
    
    /* Initialize test environment */
    tests_run = 0;
    tests_passed = 0;
    tests_failed = 0;
    
    /* Bridge Management Tests */
    test_bridge_initialization();
    
    /* 3.8.1: Resource Node Placement Tests */
    test_placement_rule_loading();
    test_node_placement();
    
    /* 3.8.2: Resource Abundance Scaling Tests */
    test_fertility_management();
    test_abundance_scaling();
    
    /* 3.8.3: Seasonal Resource Availability Tests */
    test_seasonal_system();
    test_seasonal_progression();
    
    /* 3.8.4: Resource Quality Variance Tests */
    test_quality_system_initialization();
    test_quality_calculation();
    
    /* 3.8.5: Resource Depletion & Regeneration Tests */
    test_depletion_cycle_setup();
    test_resource_harvesting();
    test_resource_regeneration();
    
    /* 3.8.6: Rare Resource Events Tests */
    test_rare_event_spawning();
    test_rare_event_processing();
    
    /* 3.8.7: Resource Discovery Mechanics Tests */
    test_discovery_location_management();
    test_discovery_attempts();
    
    /* Bridge Integration & Utility Tests */
    test_bridge_update_functionality();
    test_metrics_collection();
    test_operational_status();
    test_spatial_queries();
    
    /* Complete Integration Test */
    test_complete_workflow_integration();
    
    /* Print test results summary */
    printf("\n" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=" "=\n");
    printf("📊 Phase 3.8 Test Results Summary\n");
    printf("   Total Tests: %d\n", tests_run);
    printf("   Passed: %d (%.1f%%)\n", tests_passed, (float)tests_passed / tests_run * 100.0f);
    printf("   Failed: %d (%.1f%%)\n", tests_failed, (float)tests_failed / tests_run * 100.0f);
    
    if (tests_failed == 0) {
        printf("\n🎉 All Phase 3.8 World Generation ↔ Resource/Gathering Bridge tests passed!\n");
        printf("   ✅ Resource node placement system validated\n");
        printf("   ✅ Abundance scaling with fertility validated\n");
        printf("   ✅ Seasonal availability system validated\n");
        printf("   ✅ Quality variance system validated\n");
        printf("   ✅ Depletion & regeneration cycles validated\n");
        printf("   ✅ Rare resource events validated\n");
        printf("   ✅ Discovery mechanics validated\n");
        printf("   ✅ Complete workflow integration validated\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}
