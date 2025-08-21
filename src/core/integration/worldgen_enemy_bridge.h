/**
 * Phase 3.7: World Generation ↔ Enemy Integration Bridge
 *
 * This bridge connects world generation systems with enemy integration systems,
 * providing biome-specific encounter management, level scaling, seasonal variations,
 * spawn density control, enemy modifiers, and migration patterns.
 *
 * Key Integration Points:
 * - Biome-specific encounter table loading & application (3.7.1)
 * - Enemy level scaling based on world region difficulty (3.7.2)
 * - Seasonal enemy variations based on world generation cycles (3.7.3)
 * - Enemy pack size scaling with world region danger rating (3.7.4)
 * - Enemy modifier chances based on biome environmental factors (3.7.5)
 * - Enemy spawn density control based on world generation parameters (3.7.6)
 * - Enemy migration patterns following world resource availability (3.7.7)
 */

#pragma once

#include "event_bus.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declarations */
    typedef struct RogueWorldGenEnemyBridge RogueWorldGenEnemyBridge;

/* Constants */
#define MAX_BIOME_ENCOUNTERS 64
#define MAX_SEASONAL_VARIATIONS 16
#define MAX_ENEMY_MODIFIERS 32
#define MAX_MIGRATION_ROUTES 128
#define MAX_BIOME_TYPES 32
#define ENEMY_LEVEL_SCALING_TIERS 8

    /* Biome Types */
    typedef enum
    {
        BIOME_FOREST = 0,
        BIOME_DESERT,
        BIOME_MOUNTAINS,
        BIOME_SWAMP,
        BIOME_TUNDRA,
        BIOME_GRASSLAND,
        BIOME_CAVES,
        BIOME_VOLCANIC
    } RogueBiomeType;

    /* Season Types */
    typedef enum
    {
        SEASON_SPRING = 0,
        SEASON_SUMMER,
        SEASON_AUTUMN,
        SEASON_WINTER,
        SEASON_COUNT
    } RogueSeasonType;

    /* Enemy Modifier Types */
    typedef enum
    {
        ENEMY_MOD_HEALTH_BOOST = 0,
        ENEMY_MOD_DAMAGE_BOOST,
        ENEMY_MOD_SPEED_BOOST,
        ENEMY_MOD_FIRE_RESISTANT,
        ENEMY_MOD_ICE_RESISTANT,
        ENEMY_MOD_POISON_IMMUNE,
        ENEMY_MOD_ARMORED,
        ENEMY_MOD_BERSERKER
    } RogueEnemyModifierType;

    /* 3.7.1: Biome-Specific Encounter Management */
    typedef struct
    {
        uint32_t enemy_id;
        float spawn_weight;
        uint32_t min_level;
        uint32_t max_level;
        float difficulty_modifier;
        bool is_boss;
        uint32_t required_reputation;
    } BiomeEncounterEntry;

    typedef struct
    {
        RogueBiomeType biome_type;
        BiomeEncounterEntry encounters[MAX_BIOME_ENCOUNTERS];
        uint32_t encounter_count;
        float base_spawn_rate;
        uint32_t min_encounter_level;
        uint32_t max_encounter_level;
        bool encounters_loaded;
        uint64_t last_updated_us;
    } BiomeEncounterManager;

    /* 3.7.2: Enemy Level Scaling System */
    typedef struct
    {
        uint32_t region_id;
        float difficulty_rating;
        uint32_t base_enemy_level;
        float level_variance;
        float elite_chance;
        float boss_chance;
        uint32_t scaling_tier;
        uint64_t last_scaling_update_us;
    } RegionLevelScaling;

    /* 3.7.3: Seasonal Enemy Variations */
    typedef struct
    {
        uint32_t enemy_id;
        RogueSeasonType active_season;
        float spawn_weight_modifier;
        float health_modifier;
        float damage_modifier;
        uint32_t special_abilities;
        bool is_seasonal_exclusive;
    } SeasonalVariation;

    typedef struct
    {
        RogueSeasonType current_season;
        SeasonalVariation variations[MAX_SEASONAL_VARIATIONS];
        uint32_t variation_count;
        uint64_t season_start_time_us;
        uint64_t season_duration_us;
        bool auto_season_progression;
    } SeasonalEnemySystem;

    /* 3.7.4: Enemy Pack Size Scaling */
    typedef struct
    {
        uint32_t region_id;
        float danger_rating;
        uint32_t base_pack_size;
        uint32_t max_pack_size;
        float pack_size_variance;
        float elite_pack_chance;
        uint32_t pack_coordination_level;
    } RegionPackScaling;

    /* 3.7.5: Enemy Environmental Modifiers */
    typedef struct
    {
        RogueEnemyModifierType modifier_type;
        float activation_chance;
        float magnitude;
        uint32_t duration_seconds;
        bool stacks_with_others;
        uint32_t prerequisite_modifiers;
    } EnemyEnvironmentalModifier;

    typedef struct
    {
        RogueBiomeType biome_type;
        EnemyEnvironmentalModifier modifiers[MAX_ENEMY_MODIFIERS];
        uint32_t modifier_count;
        float environmental_harshness;
        bool modifiers_enabled;
    } BiomeModifierSystem;

    /* 3.7.6: Enemy Spawn Density Control */
    typedef struct
    {
        uint32_t region_id;
        float base_spawn_density;
        float current_spawn_density;
        float density_variance;
        uint32_t max_concurrent_enemies;
        uint32_t current_enemy_count;
        float respawn_rate_modifier;
        uint64_t last_density_update_us;
    } SpawnDensityControl;

    /* 3.7.7: Enemy Migration Patterns */
    typedef struct
    {
        uint32_t route_id;
        uint32_t source_region_id;
        uint32_t destination_region_id;
        uint32_t enemy_types[8];
        uint32_t enemy_type_count;
        float migration_trigger_threshold;
        float migration_rate;
        uint64_t migration_cooldown_us;
        uint64_t last_migration_us;
        bool is_active;
    } EnemyMigrationRoute;

    typedef struct
    {
        EnemyMigrationRoute routes[MAX_MIGRATION_ROUTES];
        uint32_t route_count;
        float global_migration_modifier;
        bool migration_enabled;
        uint64_t last_migration_check_us;
    } EnemyMigrationSystem;

    /* Performance Metrics */
    typedef struct
    {
        uint64_t encounter_table_loads;
        uint64_t level_scaling_updates;
        uint64_t seasonal_transitions;
        uint64_t pack_size_calculations;
        uint64_t modifier_applications;
        uint64_t spawn_density_updates;
        uint64_t migration_events;
        uint64_t total_operations;
        double avg_processing_time_us;
        uint64_t performance_samples;
    } WorldGenEnemyBridgeMetrics;

    /* Main Bridge Structure */
    struct RogueWorldGenEnemyBridge
    {
        /* Core Systems */
        BiomeEncounterManager encounter_managers[MAX_BIOME_TYPES];
        RegionLevelScaling level_scaling[64];
        SeasonalEnemySystem seasonal_system;
        RegionPackScaling pack_scaling[64];
        BiomeModifierSystem modifier_systems[MAX_BIOME_TYPES];
        SpawnDensityControl density_controls[64];
        EnemyMigrationSystem migration_system;

        /* Bridge State */
        bool initialized;
        bool enabled;
        uint64_t initialization_time_us;
        uint32_t active_region_count;
        uint32_t active_biome_count;

        /* Performance Tracking */
        WorldGenEnemyBridgeMetrics metrics;

        /* Event Bus Integration */
        uint32_t event_subscriber_id;
        bool event_system_connected;
    };

    /* API Functions */

    /* Bridge Management */
    bool rogue_worldgen_enemy_bridge_init(RogueWorldGenEnemyBridge* bridge);
    void rogue_worldgen_enemy_bridge_shutdown(RogueWorldGenEnemyBridge* bridge);
    bool rogue_worldgen_enemy_bridge_update(RogueWorldGenEnemyBridge* bridge, float delta_time);

    /* 3.7.1: Biome-Specific Encounter Management */
    bool rogue_worldgen_enemy_bridge_load_biome_encounters(RogueWorldGenEnemyBridge* bridge,
                                                           RogueBiomeType biome_type,
                                                           const char* encounter_table_path);
    bool rogue_worldgen_enemy_bridge_get_biome_encounter(RogueWorldGenEnemyBridge* bridge,
                                                         RogueBiomeType biome_type,
                                                         uint32_t player_level,
                                                         uint32_t* out_enemy_id,
                                                         uint32_t* out_enemy_level);

    /* 3.7.2: Enemy Level Scaling */
    bool rogue_worldgen_enemy_bridge_set_region_scaling(RogueWorldGenEnemyBridge* bridge,
                                                        uint32_t region_id, float difficulty_rating,
                                                        uint32_t base_level);
    uint32_t rogue_worldgen_enemy_bridge_get_scaled_enemy_level(RogueWorldGenEnemyBridge* bridge,
                                                                uint32_t region_id,
                                                                uint32_t base_enemy_level);

    /* 3.7.3: Seasonal Enemy Variations */
    bool rogue_worldgen_enemy_bridge_set_season(RogueWorldGenEnemyBridge* bridge,
                                                RogueSeasonType season);
    bool rogue_worldgen_enemy_bridge_add_seasonal_variation(
        RogueWorldGenEnemyBridge* bridge, uint32_t enemy_id, RogueSeasonType season,
        float spawn_modifier, float health_modifier, float damage_modifier);

    /* 3.7.4: Enemy Pack Size Scaling */
    bool rogue_worldgen_enemy_bridge_set_region_pack_scaling(RogueWorldGenEnemyBridge* bridge,
                                                             uint32_t region_id,
                                                             float danger_rating);
    uint32_t rogue_worldgen_enemy_bridge_get_pack_size(RogueWorldGenEnemyBridge* bridge,
                                                       uint32_t region_id, uint32_t base_pack_size);

    /* 3.7.5: Enemy Environmental Modifiers */
    bool rogue_worldgen_enemy_bridge_add_biome_modifier(RogueWorldGenEnemyBridge* bridge,
                                                        RogueBiomeType biome_type,
                                                        RogueEnemyModifierType modifier_type,
                                                        float chance, float magnitude);
    uint32_t rogue_worldgen_enemy_bridge_apply_environmental_modifiers(
        RogueWorldGenEnemyBridge* bridge, RogueBiomeType biome_type, uint32_t enemy_id);

    /* 3.7.6: Enemy Spawn Density Control */
    bool rogue_worldgen_enemy_bridge_set_spawn_density(RogueWorldGenEnemyBridge* bridge,
                                                       uint32_t region_id, float base_density,
                                                       uint32_t max_concurrent);
    bool rogue_worldgen_enemy_bridge_update_enemy_count(RogueWorldGenEnemyBridge* bridge,
                                                        uint32_t region_id, int32_t count_delta);

    /* 3.7.7: Enemy Migration Patterns */
    bool rogue_worldgen_enemy_bridge_add_migration_route(
        RogueWorldGenEnemyBridge* bridge, uint32_t source_region, uint32_t dest_region,
        const uint32_t* enemy_types, uint32_t type_count, float trigger_threshold);
    bool rogue_worldgen_enemy_bridge_process_migrations(RogueWorldGenEnemyBridge* bridge);

    /* Utility Functions */
    WorldGenEnemyBridgeMetrics
    rogue_worldgen_enemy_bridge_get_metrics(const RogueWorldGenEnemyBridge* bridge);
    bool rogue_worldgen_enemy_bridge_is_operational(const RogueWorldGenEnemyBridge* bridge);

#ifdef __cplusplus
}
#endif
