/**
 * Phase 3.8: World Generation ↔ Resource/Gathering Bridge
 * 
 * This bridge connects world generation systems with resource and gathering systems,
 * providing resource node placement, abundance scaling, seasonal availability,
 * quality variance, depletion cycles, rare event spawning, and discovery mechanics.
 * 
 * Key Integration Points:
 * - Resource node placement based on biome generation parameters (3.8.1)
 * - Resource abundance scaling with world region fertility (3.8.2)
 * - Seasonal resource availability changes (3.8.3)
 * - Resource quality variance based on world generation seed (3.8.4)
 * - Resource depletion & regeneration cycles tied to world time (3.8.5)
 * - Rare resource event spawning during world generation (3.8.6)
 * - Resource discovery mechanics with world exploration (3.8.7)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct RogueWorldGenResourceBridge RogueWorldGenResourceBridge;

/* Constants */
#define MAX_BIOME_RESOURCE_TYPES 32
#define MAX_RESOURCE_NODES_PER_REGION 256
#define MAX_RESOURCE_EVENTS 64
#define MAX_DISCOVERY_LOCATIONS 128
#define MAX_BIOME_TYPES 32
#define RESOURCE_QUALITY_TIERS 10

/* Biome Types */
typedef enum {
    BIOME_FOREST = 0,
    BIOME_DESERT,
    BIOME_MOUNTAINS,
    BIOME_SWAMP,
    BIOME_TUNDRA,
    BIOME_GRASSLAND,
    BIOME_CAVES,
    BIOME_VOLCANIC,
    BIOME_OCEAN
} RogueBiomeType;

/* Season Types */
typedef enum {
    SEASON_SPRING = 0,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT
} RogueSeasonType;

/* Resource Types */
typedef enum {
    RESOURCE_WOOD = 0,
    RESOURCE_STONE,
    RESOURCE_METAL_ORE,
    RESOURCE_HERBS,
    RESOURCE_GEMS,
    RESOURCE_WATER,
    RESOURCE_FOOD,
    RESOURCE_RARE_CRYSTALS,
    RESOURCE_MAGICAL_ESSENCE,
    RESOURCE_TYPE_COUNT
} RogueResourceType;

/* 3.8.1: Resource Node Placement */
typedef struct {
    uint32_t node_id;
    RogueResourceType resource_type;
    float world_x, world_y;
    uint32_t region_id;
    RogueBiomeType biome_type;
    float placement_weight;
    uint32_t max_yield;
    uint32_t current_yield;
    bool is_active;
    uint64_t created_time_us;
} ResourceNode;

typedef struct {
    RogueBiomeType biome_type;
    RogueResourceType resource_types[MAX_BIOME_RESOURCE_TYPES];
    float placement_weights[MAX_BIOME_RESOURCE_TYPES];
    uint32_t resource_type_count;
    float node_density;
    float placement_variance;
    bool placement_rules_loaded;
} BiomeResourcePlacement;

/* 3.8.2: Resource Abundance Scaling */
typedef struct {
    uint32_t region_id;
    float fertility_rating;
    float abundance_multiplier;
    float yield_variance;
    float regeneration_rate;
    uint32_t max_concurrent_nodes;
    uint32_t active_node_count;
    uint64_t last_abundance_update_us;
} RegionAbundanceScaling;

/* 3.8.3: Seasonal Resource Availability */
typedef struct {
    RogueResourceType resource_type;
    RogueSeasonType peak_season;
    float availability_modifiers[SEASON_COUNT];
    float growth_rate_modifiers[SEASON_COUNT];
    float quality_modifiers[SEASON_COUNT];
    bool is_seasonal_exclusive;
} SeasonalResourceAvailability;

typedef struct {
    RogueSeasonType current_season;
    SeasonalResourceAvailability availabilities[RESOURCE_TYPE_COUNT];
    uint32_t availability_count;
    uint64_t season_start_time_us;
    uint64_t season_duration_us;
    bool auto_season_progression;
} SeasonalResourceSystem;

/* 3.8.4: Resource Quality Variance */
typedef struct {
    uint32_t world_generation_seed;
    float quality_base_multiplier;
    float quality_variance_factor;
    uint32_t quality_distribution[RESOURCE_QUALITY_TIERS];
    float tier_probabilities[RESOURCE_QUALITY_TIERS];
    bool quality_system_initialized;
} ResourceQualitySystem;

typedef struct {
    uint32_t node_id;
    RogueResourceType resource_type;
    uint32_t base_quality;
    uint32_t current_quality;
    float quality_decay_rate;
    uint32_t quality_tier;
    float bonus_multiplier;
    uint64_t last_quality_update_us;
} ResourceQualityInstance;

/* 3.8.5: Resource Depletion & Regeneration */
typedef struct {
    uint32_t node_id;
    uint32_t max_capacity;
    uint32_t current_capacity;
    uint32_t depletion_rate;
    uint32_t regeneration_rate;
    float regeneration_efficiency;
    uint64_t last_harvest_time_us;
    uint64_t next_regeneration_time_us;
    bool is_depleted;
    bool can_regenerate;
} ResourceDepletionCycle;

/* 3.8.6: Rare Resource Events */
typedef enum {
    RARE_EVENT_CRYSTAL_BLOOM = 0,
    RARE_EVENT_METAL_VEIN_DISCOVERY,
    RARE_EVENT_ANCIENT_GROVE,
    RARE_EVENT_MAGICAL_SPRING,
    RARE_EVENT_GEM_CLUSTER,
    RARE_EVENT_VOLCANIC_ERUPTION,
    RARE_EVENT_TYPE_COUNT
} RareResourceEventType;

typedef struct {
    uint32_t event_id;
    RareResourceEventType event_type;
    uint32_t region_id;
    float world_x, world_y;
    RogueResourceType bonus_resource_type;
    uint32_t bonus_yield;
    float bonus_quality_multiplier;
    uint64_t event_start_time_us;
    uint64_t event_duration_us;
    bool is_active;
    bool has_been_discovered;
} RareResourceEvent;

typedef struct {
    RareResourceEvent events[MAX_RESOURCE_EVENTS];
    uint32_t event_count;
    uint32_t active_event_count;
    float global_event_frequency;
    uint64_t last_event_spawn_time_us;
    bool events_enabled;
} RareResourceEventSystem;

/* 3.8.7: Resource Discovery Mechanics */
typedef struct {
    uint32_t location_id;
    float world_x, world_y;
    uint32_t region_id;
    RogueResourceType hidden_resource_type;
    uint32_t discovery_difficulty;
    float discovery_radius;
    bool requires_tool;
    uint32_t required_skill_level;
    bool has_been_discovered;
    uint64_t discovery_time_us;
} ResourceDiscoveryLocation;

typedef struct {
    ResourceDiscoveryLocation locations[MAX_DISCOVERY_LOCATIONS];
    uint32_t location_count;
    uint32_t discovered_count;
    float discovery_success_rate;
    uint32_t base_discovery_xp;
    bool discovery_system_enabled;
} ResourceDiscoverySystem;

/* Performance Metrics */
typedef struct {
    uint64_t node_placements;
    uint64_t abundance_calculations;
    uint64_t seasonal_updates;
    uint64_t quality_calculations;
    uint64_t depletion_cycles;
    uint64_t rare_events_spawned;
    uint64_t discoveries_made;
    uint64_t total_operations;
    double avg_processing_time_us;
    uint64_t performance_samples;
} WorldGenResourceBridgeMetrics;

/* Main Bridge Structure */
struct RogueWorldGenResourceBridge {
    /* Core Systems */
    BiomeResourcePlacement placements[MAX_BIOME_TYPES];
    ResourceNode resource_nodes[MAX_RESOURCE_NODES_PER_REGION * 64]; /* 64 max regions */
    uint32_t total_node_count;
    RegionAbundanceScaling abundance_scaling[64];
    SeasonalResourceSystem seasonal_system;
    ResourceQualitySystem quality_system;
    ResourceQualityInstance quality_instances[MAX_RESOURCE_NODES_PER_REGION * 64];
    uint32_t quality_instance_count;
    ResourceDepletionCycle depletion_cycles[MAX_RESOURCE_NODES_PER_REGION * 64];
    uint32_t depletion_cycle_count;
    RareResourceEventSystem event_system;
    ResourceDiscoverySystem discovery_system;
    
    /* Bridge State */
    bool initialized;
    bool enabled;
    uint64_t initialization_time_us;
    uint32_t active_region_count;
    uint32_t active_biome_count;
    
    /* Performance Tracking */
    WorldGenResourceBridgeMetrics metrics;
    
    /* Event Bus Integration */
    uint32_t event_subscriber_id;
    bool event_system_connected;
};

/* API Functions */

/* Bridge Management */
bool rogue_worldgen_resource_bridge_init(RogueWorldGenResourceBridge* bridge);
void rogue_worldgen_resource_bridge_shutdown(RogueWorldGenResourceBridge* bridge);
bool rogue_worldgen_resource_bridge_update(RogueWorldGenResourceBridge* bridge, float delta_time);

/* 3.8.1: Resource Node Placement */
bool rogue_worldgen_resource_bridge_load_placement_rules(RogueWorldGenResourceBridge* bridge,
                                                        RogueBiomeType biome_type,
                                                        const char* rules_file_path);
uint32_t rogue_worldgen_resource_bridge_place_nodes(RogueWorldGenResourceBridge* bridge,
                                                    uint32_t region_id,
                                                    RogueBiomeType biome_type,
                                                    float region_x, float region_y,
                                                    float region_width, float region_height);

/* 3.8.2: Resource Abundance Scaling */
bool rogue_worldgen_resource_bridge_set_region_fertility(RogueWorldGenResourceBridge* bridge,
                                                        uint32_t region_id,
                                                        float fertility_rating);
float rogue_worldgen_resource_bridge_get_resource_abundance(RogueWorldGenResourceBridge* bridge,
                                                           uint32_t region_id,
                                                           RogueResourceType resource_type);

/* 3.8.3: Seasonal Resource Availability */
bool rogue_worldgen_resource_bridge_set_season(RogueWorldGenResourceBridge* bridge,
                                              RogueSeasonType season);
bool rogue_worldgen_resource_bridge_add_seasonal_availability(RogueWorldGenResourceBridge* bridge,
                                                             RogueResourceType resource_type,
                                                             RogueSeasonType peak_season,
                                                             const float* season_modifiers);

/* 3.8.4: Resource Quality Variance */
bool rogue_worldgen_resource_bridge_init_quality_system(RogueWorldGenResourceBridge* bridge,
                                                       uint32_t world_seed);
uint32_t rogue_worldgen_resource_bridge_calculate_resource_quality(RogueWorldGenResourceBridge* bridge,
                                                                  uint32_t node_id,
                                                                  RogueResourceType resource_type);

/* 3.8.5: Resource Depletion & Regeneration */
bool rogue_worldgen_resource_bridge_setup_depletion_cycle(RogueWorldGenResourceBridge* bridge,
                                                         uint32_t node_id,
                                                         uint32_t max_capacity,
                                                         uint32_t regen_rate);
bool rogue_worldgen_resource_bridge_harvest_resource(RogueWorldGenResourceBridge* bridge,
                                                    uint32_t node_id,
                                                    uint32_t harvest_amount);
bool rogue_worldgen_resource_bridge_process_regeneration(RogueWorldGenResourceBridge* bridge);

/* 3.8.6: Rare Resource Events */
bool rogue_worldgen_resource_bridge_spawn_rare_event(RogueWorldGenResourceBridge* bridge,
                                                    RareResourceEventType event_type,
                                                    uint32_t region_id,
                                                    float world_x, float world_y);
bool rogue_worldgen_resource_bridge_process_rare_events(RogueWorldGenResourceBridge* bridge);

/* 3.8.7: Resource Discovery Mechanics */
bool rogue_worldgen_resource_bridge_add_discovery_location(RogueWorldGenResourceBridge* bridge,
                                                          float world_x, float world_y,
                                                          uint32_t region_id,
                                                          RogueResourceType hidden_resource,
                                                          uint32_t difficulty);
bool rogue_worldgen_resource_bridge_attempt_discovery(RogueWorldGenResourceBridge* bridge,
                                                     float player_x, float player_y,
                                                     uint32_t player_skill_level,
                                                     uint32_t* out_discovery_id);

/* Utility Functions */
WorldGenResourceBridgeMetrics rogue_worldgen_resource_bridge_get_metrics(const RogueWorldGenResourceBridge* bridge);
bool rogue_worldgen_resource_bridge_is_operational(const RogueWorldGenResourceBridge* bridge);
uint32_t rogue_worldgen_resource_bridge_get_nodes_in_radius(RogueWorldGenResourceBridge* bridge,
                                                           float center_x, float center_y,
                                                           float radius,
                                                           uint32_t* out_node_ids,
                                                           uint32_t max_nodes);

#ifdef __cplusplus
}
#endif
