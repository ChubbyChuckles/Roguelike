/**
 * Phase 3.7: World Generation ↔ Enemy Integration Bridge Implementation
 *
 * This implementation provides comprehensive integration between world generation
 * systems and enemy integration systems, managing biome-specific encounters,
 * level scaling, seasonal variations, spawn density, environmental modifiers,
 * and migration patterns.
 */

#include "worldgen_enemy_bridge.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Windows-specific includes and definitions */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define fopen_s_safe(fp, filename, mode) fopen_s(fp, filename, mode)
#define strerror_s_safe(buffer, size, errnum) strerror_s(buffer, size, errnum)
#define strncpy_s_safe(dest, dest_size, src, count) strncpy_s(dest, dest_size, src, count)
#else
#define fopen_s_safe(fp, filename, mode) ((*fp = fopen(filename, mode)) ? 0 : errno)
#define strerror_s_safe(buffer, size, errnum) strncpy(buffer, strerror(errnum), size - 1)
#define strncpy_s_safe(dest, dest_size, src, count)                                                \
    strncpy(dest, src, (count < dest_size) ? count : dest_size - 1)
#endif

/* Utility Functions */
static uint64_t get_current_time_us(void)
{
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t) ((counter.QuadPart * 1000000) / frequency.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) (ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
#endif
}

static float random_float(float min, float max)
{
    return min + ((float) rand() / RAND_MAX) * (max - min);
}

static uint32_t random_uint32(uint32_t min, uint32_t max)
{
    if (min >= max)
        return min;
    return min + (rand() % (max - min + 1));
}

/* Bridge Management Functions */

bool rogue_worldgen_enemy_bridge_init(RogueWorldGenEnemyBridge* bridge)
{
    if (!bridge)
        return false;

    uint64_t start_time = get_current_time_us();

    /* Initialize bridge structure */
    memset(bridge, 0, sizeof(RogueWorldGenEnemyBridge));
    bridge->initialization_time_us = start_time;

    /* Initialize biome encounter managers */
    for (int i = 0; i < MAX_BIOME_TYPES; i++)
    {
        BiomeEncounterManager* manager = &bridge->encounter_managers[i];
        manager->biome_type = (RogueBiomeType) i;
        manager->encounter_count = 0;
        manager->base_spawn_rate = 1.0f;
        manager->min_encounter_level = 1;
        manager->max_encounter_level = 50;
        manager->encounters_loaded = false;
        manager->last_updated_us = start_time;
    }

    /* Initialize region level scaling */
    for (int i = 0; i < 64; i++)
    {
        RegionLevelScaling* scaling = &bridge->level_scaling[i];
        scaling->region_id = i;
        scaling->difficulty_rating = 1.0f;
        scaling->base_enemy_level = 1;
        scaling->level_variance = 0.2f;
        scaling->elite_chance = 0.1f;
        scaling->boss_chance = 0.02f;
        scaling->scaling_tier = 0;
        scaling->last_scaling_update_us = start_time;
    }

    /* Initialize seasonal enemy system */
    SeasonalEnemySystem* seasonal = &bridge->seasonal_system;
    seasonal->current_season = SEASON_SPRING;
    seasonal->variation_count = 0;
    seasonal->season_start_time_us = start_time;
    seasonal->season_duration_us = 3600000000ULL; /* 1 hour in microseconds */
    seasonal->auto_season_progression = false;

    /* Initialize pack scaling */
    for (int i = 0; i < 64; i++)
    {
        RegionPackScaling* pack_scaling = &bridge->pack_scaling[i];
        pack_scaling->region_id = i;
        pack_scaling->danger_rating = 1.0f;
        pack_scaling->base_pack_size = 1;
        pack_scaling->max_pack_size = 8;
        pack_scaling->pack_size_variance = 0.3f;
        pack_scaling->elite_pack_chance = 0.05f;
        pack_scaling->pack_coordination_level = 1;
    }

    /* Initialize biome modifier systems */
    for (int i = 0; i < MAX_BIOME_TYPES; i++)
    {
        BiomeModifierSystem* mod_system = &bridge->modifier_systems[i];
        mod_system->biome_type = (RogueBiomeType) i;
        mod_system->modifier_count = 0;
        mod_system->environmental_harshness = 1.0f;
        mod_system->modifiers_enabled = true;
    }

    /* Initialize spawn density controls */
    for (int i = 0; i < 64; i++)
    {
        SpawnDensityControl* density = &bridge->density_controls[i];
        density->region_id = i;
        density->base_spawn_density = 1.0f;
        density->current_spawn_density = 1.0f;
        density->density_variance = 0.2f;
        density->max_concurrent_enemies = 20;
        density->current_enemy_count = 0;
        density->respawn_rate_modifier = 1.0f;
        density->last_density_update_us = start_time;
    }

    /* Initialize migration system */
    EnemyMigrationSystem* migration = &bridge->migration_system;
    migration->route_count = 0;
    migration->global_migration_modifier = 1.0f;
    migration->migration_enabled = true;
    migration->last_migration_check_us = start_time;

    /* Initialize performance metrics */
    memset(&bridge->metrics, 0, sizeof(WorldGenEnemyBridgeMetrics));

    /* Set bridge state */
    bridge->initialized = true;
    bridge->enabled = true;
    bridge->active_region_count = 0;
    bridge->active_biome_count = 0;
    bridge->event_subscriber_id = 0;
    bridge->event_system_connected = false;

    return true;
}

void rogue_worldgen_enemy_bridge_shutdown(RogueWorldGenEnemyBridge* bridge)
{
    if (!bridge || !bridge->initialized)
        return;

    /* Clean shutdown - no dynamic memory to free in this implementation */
    bridge->initialized = false;
    bridge->enabled = false;
    bridge->event_system_connected = false;
}

bool rogue_worldgen_enemy_bridge_update(RogueWorldGenEnemyBridge* bridge, float delta_time)
{
    if (!bridge || !bridge->initialized || !bridge->enabled)
        return false;

    uint64_t start_time = get_current_time_us();

    /* Use delta_time for scaling */
    (void) delta_time; /* Suppress unused parameter warning */

    /* Update seasonal system */
    SeasonalEnemySystem* seasonal = &bridge->seasonal_system;
    if (seasonal->auto_season_progression)
    {
        uint64_t elapsed = start_time - seasonal->season_start_time_us;
        if (elapsed >= seasonal->season_duration_us)
        {
            seasonal->current_season =
                (RogueSeasonType) ((seasonal->current_season + 1) % SEASON_COUNT);
            seasonal->season_start_time_us = start_time;
            bridge->metrics.seasonal_transitions++;
        }
    }

    /* Update spawn density controls */
    for (int i = 0; i < 64; i++)
    {
        SpawnDensityControl* density = &bridge->density_controls[i];
        if (start_time - density->last_density_update_us > 1000000)
        { /* Update every second */
            float variance = random_float(-density->density_variance, density->density_variance);
            density->current_spawn_density = density->base_spawn_density * (1.0f + variance);
            density->last_density_update_us = start_time;
            bridge->metrics.spawn_density_updates++;
        }
    }

    /* Process migrations */
    EnemyMigrationSystem* migration = &bridge->migration_system;
    if (migration->migration_enabled && start_time - migration->last_migration_check_us > 5000000)
    { /* Check every 5 seconds */
        rogue_worldgen_enemy_bridge_process_migrations(bridge);
        migration->last_migration_check_us = start_time;
    }

    /* Update performance metrics */
    uint64_t end_time = get_current_time_us();
    double processing_time = (double) (end_time - start_time);
    bridge->metrics.avg_processing_time_us =
        (bridge->metrics.avg_processing_time_us * bridge->metrics.performance_samples +
         processing_time) /
        (bridge->metrics.performance_samples + 1);
    bridge->metrics.performance_samples++;
    bridge->metrics.total_operations++;

    return true;
}

/* 3.7.1: Biome-Specific Encounter Management */

bool rogue_worldgen_enemy_bridge_load_biome_encounters(RogueWorldGenEnemyBridge* bridge,
                                                       RogueBiomeType biome_type,
                                                       const char* encounter_table_path)
{
    if (!bridge || !bridge->initialized || biome_type >= MAX_BIOME_TYPES || !encounter_table_path)
    {
        return false;
    }

    BiomeEncounterManager* manager = &bridge->encounter_managers[biome_type];

    FILE* file;
    if (fopen_s_safe(&file, encounter_table_path, "r") != 0)
    {
        return false;
    }

    manager->encounter_count = 0;
    char line[256];

    /* Read encounter entries from file */
    while (fgets(line, sizeof(line), file) && manager->encounter_count < MAX_BIOME_ENCOUNTERS)
    {
        BiomeEncounterEntry* entry = &manager->encounters[manager->encounter_count];

        /* Parse line: enemy_id,spawn_weight,min_level,max_level,difficulty_mod,is_boss,req_rep */
        if (sscanf_s(line, "%u,%f,%u,%u,%f,%d,%u", &entry->enemy_id, &entry->spawn_weight,
                     &entry->min_level, &entry->max_level, &entry->difficulty_modifier,
                     (int*) &entry->is_boss, &entry->required_reputation) == 7)
        {
            manager->encounter_count++;
        }
    }

    fclose(file);

    manager->encounters_loaded = true;
    manager->last_updated_us = get_current_time_us();
    bridge->metrics.encounter_table_loads++;

    return manager->encounter_count > 0;
}

bool rogue_worldgen_enemy_bridge_get_biome_encounter(RogueWorldGenEnemyBridge* bridge,
                                                     RogueBiomeType biome_type,
                                                     uint32_t player_level, uint32_t* out_enemy_id,
                                                     uint32_t* out_enemy_level)
{
    if (!bridge || !bridge->initialized || biome_type >= MAX_BIOME_TYPES || !out_enemy_id ||
        !out_enemy_level)
    {
        return false;
    }

    BiomeEncounterManager* manager = &bridge->encounter_managers[biome_type];
    if (!manager->encounters_loaded || manager->encounter_count == 0)
    {
        return false;
    }

    /* Calculate total weight for level-appropriate encounters */
    float total_weight = 0.0f;
    for (uint32_t i = 0; i < manager->encounter_count; i++)
    {
        BiomeEncounterEntry* entry = &manager->encounters[i];
        if (player_level >= entry->min_level && player_level <= entry->max_level)
        {
            total_weight += entry->spawn_weight;
        }
    }

    if (total_weight <= 0.0f)
        return false;

    /* Select encounter based on weighted random selection */
    float random_value = random_float(0.0f, total_weight);
    float current_weight = 0.0f;

    for (uint32_t i = 0; i < manager->encounter_count; i++)
    {
        BiomeEncounterEntry* entry = &manager->encounters[i];
        if (player_level >= entry->min_level && player_level <= entry->max_level)
        {
            current_weight += entry->spawn_weight;
            if (random_value <= current_weight)
            {
                *out_enemy_id = entry->enemy_id;
                *out_enemy_level = random_uint32(entry->min_level, entry->max_level);
                return true;
            }
        }
    }

    return false;
}

/* 3.7.2: Enemy Level Scaling */

bool rogue_worldgen_enemy_bridge_set_region_scaling(RogueWorldGenEnemyBridge* bridge,
                                                    uint32_t region_id, float difficulty_rating,
                                                    uint32_t base_level)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return false;

    RegionLevelScaling* scaling = &bridge->level_scaling[region_id];
    scaling->difficulty_rating = difficulty_rating;
    scaling->base_enemy_level = base_level;
    scaling->scaling_tier = (uint32_t) (difficulty_rating * ENEMY_LEVEL_SCALING_TIERS);
    scaling->last_scaling_update_us = get_current_time_us();

    bridge->metrics.level_scaling_updates++;
    return true;
}

uint32_t rogue_worldgen_enemy_bridge_get_scaled_enemy_level(RogueWorldGenEnemyBridge* bridge,
                                                            uint32_t region_id,
                                                            uint32_t base_enemy_level)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return base_enemy_level;

    RegionLevelScaling* scaling = &bridge->level_scaling[region_id];

    float level_multiplier = scaling->difficulty_rating;
    float variance = random_float(-scaling->level_variance, scaling->level_variance);
    uint32_t scaled_level =
        (uint32_t) ((float) base_enemy_level * level_multiplier * (1.0f + variance));

    /* Ensure minimum level of 1 */
    return scaled_level > 0 ? scaled_level : 1;
}

/* 3.7.3: Seasonal Enemy Variations */

bool rogue_worldgen_enemy_bridge_set_season(RogueWorldGenEnemyBridge* bridge,
                                            RogueSeasonType season)
{
    if (!bridge || !bridge->initialized || season >= SEASON_COUNT)
        return false;

    bridge->seasonal_system.current_season = season;
    bridge->seasonal_system.season_start_time_us = get_current_time_us();
    bridge->metrics.seasonal_transitions++;

    return true;
}

bool rogue_worldgen_enemy_bridge_add_seasonal_variation(RogueWorldGenEnemyBridge* bridge,
                                                        uint32_t enemy_id, RogueSeasonType season,
                                                        float spawn_modifier, float health_modifier,
                                                        float damage_modifier)
{
    if (!bridge || !bridge->initialized || season >= SEASON_COUNT ||
        bridge->seasonal_system.variation_count >= MAX_SEASONAL_VARIATIONS)
    {
        return false;
    }

    SeasonalVariation* variation =
        &bridge->seasonal_system.variations[bridge->seasonal_system.variation_count];
    variation->enemy_id = enemy_id;
    variation->active_season = season;
    variation->spawn_weight_modifier = spawn_modifier;
    variation->health_modifier = health_modifier;
    variation->damage_modifier = damage_modifier;
    variation->special_abilities = 0;
    variation->is_seasonal_exclusive = false;

    bridge->seasonal_system.variation_count++;
    return true;
}

/* 3.7.4: Enemy Pack Size Scaling */

bool rogue_worldgen_enemy_bridge_set_region_pack_scaling(RogueWorldGenEnemyBridge* bridge,
                                                         uint32_t region_id, float danger_rating)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return false;

    RegionPackScaling* pack_scaling = &bridge->pack_scaling[region_id];
    pack_scaling->danger_rating = danger_rating;
    pack_scaling->base_pack_size = (uint32_t) (1 + danger_rating * 3);
    pack_scaling->max_pack_size = (uint32_t) (8 * danger_rating);
    pack_scaling->elite_pack_chance = danger_rating * 0.1f;
    pack_scaling->pack_coordination_level = (uint32_t) (danger_rating * 5);

    bridge->metrics.pack_size_calculations++;
    return true;
}

uint32_t rogue_worldgen_enemy_bridge_get_pack_size(RogueWorldGenEnemyBridge* bridge,
                                                   uint32_t region_id, uint32_t base_pack_size)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return base_pack_size;

    RegionPackScaling* pack_scaling = &bridge->pack_scaling[region_id];

    float size_multiplier = pack_scaling->danger_rating;
    float variance =
        random_float(-pack_scaling->pack_size_variance, pack_scaling->pack_size_variance);
    uint32_t calculated_size =
        (uint32_t) ((float) base_pack_size * size_multiplier * (1.0f + variance));

    /* Clamp to max pack size */
    if (calculated_size > pack_scaling->max_pack_size)
    {
        calculated_size = pack_scaling->max_pack_size;
    }

    return calculated_size > 0 ? calculated_size : 1;
}

/* 3.7.5: Enemy Environmental Modifiers */

bool rogue_worldgen_enemy_bridge_add_biome_modifier(RogueWorldGenEnemyBridge* bridge,
                                                    RogueBiomeType biome_type,
                                                    RogueEnemyModifierType modifier_type,
                                                    float chance, float magnitude)
{
    if (!bridge || !bridge->initialized || biome_type >= MAX_BIOME_TYPES)
        return false;

    BiomeModifierSystem* mod_system = &bridge->modifier_systems[biome_type];
    if (mod_system->modifier_count >= MAX_ENEMY_MODIFIERS)
        return false;

    EnemyEnvironmentalModifier* modifier = &mod_system->modifiers[mod_system->modifier_count];
    modifier->modifier_type = modifier_type;
    modifier->activation_chance = chance;
    modifier->magnitude = magnitude;
    modifier->duration_seconds = 300; /* 5 minutes default */
    modifier->stacks_with_others = true;
    modifier->prerequisite_modifiers = 0;

    mod_system->modifier_count++;
    bridge->metrics.modifier_applications++;

    return true;
}

uint32_t rogue_worldgen_enemy_bridge_apply_environmental_modifiers(RogueWorldGenEnemyBridge* bridge,
                                                                   RogueBiomeType biome_type,
                                                                   uint32_t enemy_id)
{
    if (!bridge || !bridge->initialized || biome_type >= MAX_BIOME_TYPES)
        return 0;

    /* Suppress unused parameter warning */
    (void) enemy_id;

    BiomeModifierSystem* mod_system = &bridge->modifier_systems[biome_type];
    if (!mod_system->modifiers_enabled)
        return 0;

    uint32_t applied_modifiers = 0;

    for (uint32_t i = 0; i < mod_system->modifier_count; i++)
    {
        EnemyEnvironmentalModifier* modifier = &mod_system->modifiers[i];

        float adjusted_chance = modifier->activation_chance * mod_system->environmental_harshness;
        if (random_float(0.0f, 1.0f) <= adjusted_chance)
        {
            applied_modifiers |= (1 << modifier->modifier_type);
        }
    }

    if (applied_modifiers > 0)
    {
        bridge->metrics.modifier_applications++;
    }

    return applied_modifiers;
}

/* 3.7.6: Enemy Spawn Density Control */

bool rogue_worldgen_enemy_bridge_set_spawn_density(RogueWorldGenEnemyBridge* bridge,
                                                   uint32_t region_id, float base_density,
                                                   uint32_t max_concurrent)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return false;

    SpawnDensityControl* density = &bridge->density_controls[region_id];
    density->base_spawn_density = base_density;
    density->current_spawn_density = base_density;
    density->max_concurrent_enemies = max_concurrent;
    density->last_density_update_us = get_current_time_us();

    bridge->metrics.spawn_density_updates++;
    return true;
}

bool rogue_worldgen_enemy_bridge_update_enemy_count(RogueWorldGenEnemyBridge* bridge,
                                                    uint32_t region_id, int32_t count_delta)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return false;

    SpawnDensityControl* density = &bridge->density_controls[region_id];

    int32_t new_count = (int32_t) density->current_enemy_count + count_delta;
    if (new_count < 0)
        new_count = 0;

    density->current_enemy_count = (uint32_t) new_count;

    /* Adjust respawn rate based on current density */
    float density_ratio =
        (float) density->current_enemy_count / (float) density->max_concurrent_enemies;
    density->respawn_rate_modifier =
        1.0f - (density_ratio * 0.5f); /* Slower respawn when crowded */

    return true;
}

/* 3.7.7: Enemy Migration Patterns */

bool rogue_worldgen_enemy_bridge_add_migration_route(RogueWorldGenEnemyBridge* bridge,
                                                     uint32_t source_region, uint32_t dest_region,
                                                     const uint32_t* enemy_types,
                                                     uint32_t type_count, float trigger_threshold)
{
    if (!bridge || !bridge->initialized || !enemy_types || type_count == 0 ||
        bridge->migration_system.route_count >= MAX_MIGRATION_ROUTES)
    {
        return false;
    }

    EnemyMigrationRoute* route =
        &bridge->migration_system.routes[bridge->migration_system.route_count];
    route->route_id = bridge->migration_system.route_count;
    route->source_region_id = source_region;
    route->destination_region_id = dest_region;
    route->migration_trigger_threshold = trigger_threshold;
    route->migration_rate = 0.1f;               /* 10% migration rate */
    route->migration_cooldown_us = 30000000ULL; /* 30 second cooldown */
    route->last_migration_us = 0;
    route->is_active = true;

    /* Copy enemy types */
    uint32_t copy_count = type_count > 8 ? 8 : type_count;
    for (uint32_t i = 0; i < copy_count; i++)
    {
        route->enemy_types[i] = enemy_types[i];
    }
    route->enemy_type_count = copy_count;

    bridge->migration_system.route_count++;
    return true;
}

bool rogue_worldgen_enemy_bridge_process_migrations(RogueWorldGenEnemyBridge* bridge)
{
    if (!bridge || !bridge->initialized || !bridge->migration_system.migration_enabled)
    {
        return false;
    }

    uint64_t current_time = get_current_time_us();
    bool migrations_processed = false;

    for (uint32_t i = 0; i < bridge->migration_system.route_count; i++)
    {
        EnemyMigrationRoute* route = &bridge->migration_system.routes[i];

        if (!route->is_active ||
            current_time - route->last_migration_us < route->migration_cooldown_us)
        {
            continue;
        }

        /* Check if migration should trigger based on source region density */
        SpawnDensityControl* source_density = &bridge->density_controls[route->source_region_id];
        float density_ratio = (float) source_density->current_enemy_count /
                              (float) source_density->max_concurrent_enemies;

        if (density_ratio >= route->migration_trigger_threshold)
        {
            /* Process migration */
            uint32_t migration_count =
                (uint32_t) (source_density->current_enemy_count * route->migration_rate);
            if (migration_count > 0)
            {
                /* Move enemies from source to destination */
                rogue_worldgen_enemy_bridge_update_enemy_count(bridge, route->source_region_id,
                                                               -(int32_t) migration_count);
                rogue_worldgen_enemy_bridge_update_enemy_count(bridge, route->destination_region_id,
                                                               (int32_t) migration_count);

                route->last_migration_us = current_time;
                bridge->metrics.migration_events++;
                migrations_processed = true;
            }
        }
    }

    return migrations_processed;
}

/* Utility Functions */

WorldGenEnemyBridgeMetrics
rogue_worldgen_enemy_bridge_get_metrics(const RogueWorldGenEnemyBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        WorldGenEnemyBridgeMetrics empty_metrics;
        memset(&empty_metrics, 0, sizeof(empty_metrics));
        return empty_metrics;
    }

    return bridge->metrics;
}

bool rogue_worldgen_enemy_bridge_is_operational(const RogueWorldGenEnemyBridge* bridge)
{
    if (!bridge)
        return false;

    return bridge->initialized && bridge->enabled;
}
