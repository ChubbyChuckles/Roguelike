/**
 * Phase 3.8: World Generation ↔ Resource/Gathering Bridge Implementation
 *
 * This implementation provides comprehensive integration between world generation
 * and resource/gathering systems, managing resource node placement, abundance scaling,
 * seasonal availability, quality variance, depletion cycles, rare events, and discovery.
 */

#include "worldgen_resource_bridge.h"
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

static float distance_2d(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    return sqrtf(dx * dx + dy * dy);
}

/* Bridge Management Functions */

bool rogue_worldgen_resource_bridge_init(RogueWorldGenResourceBridge* bridge)
{
    if (!bridge)
        return false;

    uint64_t start_time = get_current_time_us();

    /* Initialize bridge structure */
    memset(bridge, 0, sizeof(RogueWorldGenResourceBridge));
    bridge->initialization_time_us = start_time;

    /* Initialize biome resource placements */
    for (int i = 0; i < MAX_BIOME_TYPES; i++)
    {
        BiomeResourcePlacement* placement = &bridge->placements[i];
        placement->biome_type = (RogueBiomeType) i;
        placement->resource_type_count = 0;
        placement->node_density = 1.0f;
        placement->placement_variance = 0.3f;
        placement->placement_rules_loaded = false;
    }

    /* Initialize region abundance scaling */
    for (int i = 0; i < 64; i++)
    {
        RegionAbundanceScaling* abundance = &bridge->abundance_scaling[i];
        abundance->region_id = i;
        abundance->fertility_rating = 1.0f;
        abundance->abundance_multiplier = 1.0f;
        abundance->yield_variance = 0.2f;
        abundance->regeneration_rate = 1.0f;
        abundance->max_concurrent_nodes = 50;
        abundance->active_node_count = 0;
        abundance->last_abundance_update_us = start_time;
    }

    /* Initialize seasonal resource system */
    SeasonalResourceSystem* seasonal = &bridge->seasonal_system;
    seasonal->current_season = SEASON_SPRING;
    seasonal->availability_count = 0;
    seasonal->season_start_time_us = start_time;
    seasonal->season_duration_us = 3600000000ULL; /* 1 hour in microseconds */
    seasonal->auto_season_progression = false;

    /* Initialize quality system */
    ResourceQualitySystem* quality = &bridge->quality_system;
    quality->world_generation_seed = 12345;
    quality->quality_base_multiplier = 1.0f;
    quality->quality_variance_factor = 0.3f;
    quality->quality_system_initialized = false;

    /* Initialize equal distribution across quality tiers */
    for (int i = 0; i < RESOURCE_QUALITY_TIERS; i++)
    {
        quality->tier_probabilities[i] = 1.0f / RESOURCE_QUALITY_TIERS;
    }

    /* Initialize rare event system */
    RareResourceEventSystem* events = &bridge->event_system;
    events->event_count = 0;
    events->active_event_count = 0;
    events->global_event_frequency = 0.01f; /* 1% chance per update */
    events->last_event_spawn_time_us = start_time;
    events->events_enabled = true;

    /* Initialize discovery system */
    ResourceDiscoverySystem* discovery = &bridge->discovery_system;
    discovery->location_count = 0;
    discovery->discovered_count = 0;
    discovery->discovery_success_rate = 0.8f;
    discovery->base_discovery_xp = 100;
    discovery->discovery_system_enabled = true;

    /* Initialize counters */
    bridge->total_node_count = 0;
    bridge->quality_instance_count = 0;
    bridge->depletion_cycle_count = 0;

    /* Initialize performance metrics */
    memset(&bridge->metrics, 0, sizeof(WorldGenResourceBridgeMetrics));

    /* Set bridge state */
    bridge->initialized = true;
    bridge->enabled = true;
    bridge->active_region_count = 0;
    bridge->active_biome_count = 0;
    bridge->event_subscriber_id = 0;
    bridge->event_system_connected = false;

    return true;
}

void rogue_worldgen_resource_bridge_shutdown(RogueWorldGenResourceBridge* bridge)
{
    if (!bridge || !bridge->initialized)
        return;

    /* Clean shutdown - no dynamic memory to free in this implementation */
    bridge->initialized = false;
    bridge->enabled = false;
    bridge->event_system_connected = false;
}

bool rogue_worldgen_resource_bridge_update(RogueWorldGenResourceBridge* bridge, float delta_time)
{
    if (!bridge || !bridge->initialized || !bridge->enabled)
        return false;

    uint64_t start_time = get_current_time_us();

    /* Suppress unused parameter warning */
    (void) delta_time;

    /* Update seasonal system */
    SeasonalResourceSystem* seasonal = &bridge->seasonal_system;
    if (seasonal->auto_season_progression)
    {
        uint64_t elapsed = start_time - seasonal->season_start_time_us;
        if (elapsed >= seasonal->season_duration_us)
        {
            seasonal->current_season =
                (RogueSeasonType) ((seasonal->current_season + 1) % SEASON_COUNT);
            seasonal->season_start_time_us = start_time;
            bridge->metrics.seasonal_updates++;
        }
    }

    /* Process resource regeneration */
    rogue_worldgen_resource_bridge_process_regeneration(bridge);

    /* Process rare events */
    if (bridge->event_system.events_enabled)
    {
        rogue_worldgen_resource_bridge_process_rare_events(bridge);
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

/* 3.8.1: Resource Node Placement */

bool rogue_worldgen_resource_bridge_load_placement_rules(RogueWorldGenResourceBridge* bridge,
                                                         RogueBiomeType biome_type,
                                                         const char* rules_file_path)
{
    if (!bridge || !bridge->initialized || biome_type >= MAX_BIOME_TYPES || !rules_file_path)
    {
        return false;
    }

    BiomeResourcePlacement* placement = &bridge->placements[biome_type];

    FILE* file;
    if (fopen_s_safe(&file, rules_file_path, "r") != 0)
    {
        return false;
    }

    placement->resource_type_count = 0;
    char line[256];

    /* Read placement rules from file */
    while (fgets(line, sizeof(line), file) &&
           placement->resource_type_count < MAX_BIOME_RESOURCE_TYPES)
    {
        int resource_type_int;
        float weight;

        /* Parse line: resource_type,weight */
        if (sscanf_s(line, "%d,%f", &resource_type_int, &weight) == 2)
        {
            if (resource_type_int >= 0 && resource_type_int < RESOURCE_TYPE_COUNT)
            {
                placement->resource_types[placement->resource_type_count] =
                    (RogueResourceType) resource_type_int;
                placement->placement_weights[placement->resource_type_count] = weight;
                placement->resource_type_count++;
            }
        }
    }

    fclose(file);

    placement->placement_rules_loaded = true;
    return placement->resource_type_count > 0;
}

uint32_t rogue_worldgen_resource_bridge_place_nodes(RogueWorldGenResourceBridge* bridge,
                                                    uint32_t region_id, RogueBiomeType biome_type,
                                                    float region_x, float region_y,
                                                    float region_width, float region_height)
{
    if (!bridge || !bridge->initialized || biome_type >= MAX_BIOME_TYPES || region_id >= 64)
    {
        return 0;
    }

    BiomeResourcePlacement* placement = &bridge->placements[biome_type];
    if (!placement->placement_rules_loaded || placement->resource_type_count == 0)
    {
        return 0;
    }

    RegionAbundanceScaling* abundance = &bridge->abundance_scaling[region_id];
    float density_calculation = placement->node_density * abundance->abundance_multiplier *
                                (region_width * region_height) / 10000.0f; /* Normalize by area */
    uint32_t target_nodes = (uint32_t) density_calculation;

    uint32_t nodes_placed = 0;

    for (uint32_t i = 0;
         i < target_nodes && bridge->total_node_count < MAX_RESOURCE_NODES_PER_REGION * 64; i++)
    {
        /* Select random resource type based on weights */
        float total_weight = 0.0f;
        for (uint32_t j = 0; j < placement->resource_type_count; j++)
        {
            total_weight += placement->placement_weights[j];
        }

        float random_value = random_float(0.0f, total_weight);
        float current_weight = 0.0f;
        RogueResourceType selected_type = placement->resource_types[0];

        for (uint32_t j = 0; j < placement->resource_type_count; j++)
        {
            current_weight += placement->placement_weights[j];
            if (random_value <= current_weight)
            {
                selected_type = placement->resource_types[j];
                break;
            }
        }

        /* Place node at random location within region */
        ResourceNode* node = &bridge->resource_nodes[bridge->total_node_count];
        node->node_id = bridge->total_node_count;
        node->resource_type = selected_type;
        node->world_x = region_x + random_float(0.0f, region_width);
        node->world_y = region_y + random_float(0.0f, region_height);
        node->region_id = region_id;
        node->biome_type = biome_type;
        node->placement_weight = placement->placement_weights[0]; /* Use first weight as default */
        node->max_yield = random_uint32(10, 100);
        node->current_yield = node->max_yield;
        node->is_active = true;
        node->created_time_us = get_current_time_us();

        bridge->total_node_count++;
        nodes_placed++;
        abundance->active_node_count++;
    }

    bridge->metrics.node_placements += nodes_placed;
    return nodes_placed;
}

/* 3.8.2: Resource Abundance Scaling */

bool rogue_worldgen_resource_bridge_set_region_fertility(RogueWorldGenResourceBridge* bridge,
                                                         uint32_t region_id, float fertility_rating)
{
    if (!bridge || !bridge->initialized || region_id >= 64)
        return false;

    RegionAbundanceScaling* abundance = &bridge->abundance_scaling[region_id];
    abundance->fertility_rating = fertility_rating;
    abundance->abundance_multiplier = fertility_rating;
    abundance->regeneration_rate = fertility_rating * 1.2f; /* Fertile regions regenerate faster */
    abundance->last_abundance_update_us = get_current_time_us();

    bridge->metrics.abundance_calculations++;
    return true;
}

float rogue_worldgen_resource_bridge_get_resource_abundance(RogueWorldGenResourceBridge* bridge,
                                                            uint32_t region_id,
                                                            RogueResourceType resource_type)
{
    if (!bridge || !bridge->initialized || region_id >= 64 || resource_type >= RESOURCE_TYPE_COUNT)
    {
        return 0.0f;
    }

    /* Suppress unused parameter warning */
    (void) resource_type;

    RegionAbundanceScaling* abundance = &bridge->abundance_scaling[region_id];

    /* Apply seasonal modifiers */
    SeasonalResourceSystem* seasonal = &bridge->seasonal_system;
    float seasonal_modifier = 1.0f;

    for (uint32_t i = 0; i < seasonal->availability_count; i++)
    {
        if (seasonal->availabilities[i].resource_type == resource_type)
        {
            seasonal_modifier =
                seasonal->availabilities[i].availability_modifiers[seasonal->current_season];
            break;
        }
    }

    return abundance->abundance_multiplier * seasonal_modifier;
}

/* 3.8.3: Seasonal Resource Availability */

bool rogue_worldgen_resource_bridge_set_season(RogueWorldGenResourceBridge* bridge,
                                               RogueSeasonType season)
{
    if (!bridge || !bridge->initialized || season >= SEASON_COUNT)
        return false;

    bridge->seasonal_system.current_season = season;
    bridge->seasonal_system.season_start_time_us = get_current_time_us();
    bridge->metrics.seasonal_updates++;

    return true;
}

bool rogue_worldgen_resource_bridge_add_seasonal_availability(RogueWorldGenResourceBridge* bridge,
                                                              RogueResourceType resource_type,
                                                              RogueSeasonType peak_season,
                                                              const float* season_modifiers)
{
    if (!bridge || !bridge->initialized || resource_type >= RESOURCE_TYPE_COUNT ||
        peak_season >= SEASON_COUNT || !season_modifiers)
    {
        return false;
    }

    SeasonalResourceSystem* seasonal = &bridge->seasonal_system;
    if (seasonal->availability_count >= RESOURCE_TYPE_COUNT)
        return false;

    SeasonalResourceAvailability* availability =
        &seasonal->availabilities[seasonal->availability_count];
    availability->resource_type = resource_type;
    availability->peak_season = peak_season;
    availability->is_seasonal_exclusive = false;

    for (int i = 0; i < SEASON_COUNT; i++)
    {
        availability->availability_modifiers[i] = season_modifiers[i];
        availability->growth_rate_modifiers[i] =
            season_modifiers[i] * 0.8f; /* Growth rate slightly lower */
        availability->quality_modifiers[i] =
            season_modifiers[i] * 1.1f; /* Quality slightly higher */
    }

    seasonal->availability_count++;
    return true;
}

/* 3.8.4: Resource Quality Variance */

bool rogue_worldgen_resource_bridge_init_quality_system(RogueWorldGenResourceBridge* bridge,
                                                        uint32_t world_seed)
{
    if (!bridge || !bridge->initialized)
        return false;

    ResourceQualitySystem* quality = &bridge->quality_system;
    quality->world_generation_seed = world_seed;
    quality->quality_system_initialized = true;

    /* Seed random number generator for consistent quality distribution */
    srand(world_seed);

    /* Initialize quality distribution based on seed */
    for (int i = 0; i < RESOURCE_QUALITY_TIERS; i++)
    {
        quality->quality_distribution[i] = random_uint32(1, 100);

        /* Higher tiers are progressively rarer */
        float rarity_factor = 1.0f - ((float) i / RESOURCE_QUALITY_TIERS);
        quality->tier_probabilities[i] = rarity_factor * rarity_factor;
    }

    /* Normalize probabilities */
    float total_probability = 0.0f;
    for (int i = 0; i < RESOURCE_QUALITY_TIERS; i++)
    {
        total_probability += quality->tier_probabilities[i];
    }

    for (int i = 0; i < RESOURCE_QUALITY_TIERS; i++)
    {
        quality->tier_probabilities[i] /= total_probability;
    }

    bridge->metrics.quality_calculations++;
    return true;
}

uint32_t rogue_worldgen_resource_bridge_calculate_resource_quality(
    RogueWorldGenResourceBridge* bridge, uint32_t node_id, RogueResourceType resource_type)
{
    if (!bridge || !bridge->initialized || node_id >= bridge->total_node_count ||
        resource_type >= RESOURCE_TYPE_COUNT)
    {
        return 1; /* Minimum quality */
    }

    ResourceQualitySystem* quality = &bridge->quality_system;
    if (!quality->quality_system_initialized)
    {
        return 1;
    }

    /* Find or create quality instance */
    ResourceQualityInstance* instance = NULL;
    for (uint32_t i = 0; i < bridge->quality_instance_count; i++)
    {
        if (bridge->quality_instances[i].node_id == node_id)
        {
            instance = &bridge->quality_instances[i];
            break;
        }
    }

    /* Create new instance if not found */
    if (!instance && bridge->quality_instance_count < MAX_RESOURCE_NODES_PER_REGION * 64)
    {
        instance = &bridge->quality_instances[bridge->quality_instance_count];
        instance->node_id = node_id;
        instance->resource_type = resource_type;
        instance->quality_decay_rate = 0.01f; /* 1% decay per day */
        instance->last_quality_update_us = get_current_time_us();

        /* Calculate base quality using weighted random selection */
        float random_value = random_float(0.0f, 1.0f);
        float cumulative_probability = 0.0f;
        uint32_t selected_tier = 0;

        for (int i = 0; i < RESOURCE_QUALITY_TIERS; i++)
        {
            cumulative_probability += quality->tier_probabilities[i];
            if (random_value <= cumulative_probability)
            {
                selected_tier = i;
                break;
            }
        }

        instance->base_quality = selected_tier * 10 + random_uint32(1, 10); /* Quality 1-100 */
        instance->current_quality = instance->base_quality;
        instance->quality_tier = selected_tier;
        instance->bonus_multiplier = 1.0f + (selected_tier * 0.1f); /* 10% bonus per tier */

        bridge->quality_instance_count++;
        bridge->metrics.quality_calculations++;
    }

    return instance ? instance->current_quality : 1;
}

/* 3.8.5: Resource Depletion & Regeneration */

bool rogue_worldgen_resource_bridge_setup_depletion_cycle(RogueWorldGenResourceBridge* bridge,
                                                          uint32_t node_id, uint32_t max_capacity,
                                                          uint32_t regen_rate)
{
    if (!bridge || !bridge->initialized || node_id >= bridge->total_node_count ||
        bridge->depletion_cycle_count >= MAX_RESOURCE_NODES_PER_REGION * 64)
    {
        return false;
    }

    ResourceDepletionCycle* cycle = &bridge->depletion_cycles[bridge->depletion_cycle_count];
    cycle->node_id = node_id;
    cycle->max_capacity = max_capacity;
    cycle->current_capacity = max_capacity;
    cycle->depletion_rate = 1; /* 1 resource per harvest */
    cycle->regeneration_rate = regen_rate;
    cycle->regeneration_efficiency = 1.0f;
    cycle->last_harvest_time_us = 0;
    cycle->next_regeneration_time_us = get_current_time_us() + 1000000; /* 1 second */
    cycle->is_depleted = false;
    cycle->can_regenerate = true;

    bridge->depletion_cycle_count++;
    bridge->metrics.depletion_cycles++;

    return true;
}

bool rogue_worldgen_resource_bridge_harvest_resource(RogueWorldGenResourceBridge* bridge,
                                                     uint32_t node_id, uint32_t harvest_amount)
{
    if (!bridge || !bridge->initialized || node_id >= bridge->total_node_count)
    {
        return false;
    }

    /* Find depletion cycle for node */
    ResourceDepletionCycle* cycle = NULL;
    for (uint32_t i = 0; i < bridge->depletion_cycle_count; i++)
    {
        if (bridge->depletion_cycles[i].node_id == node_id)
        {
            cycle = &bridge->depletion_cycles[i];
            break;
        }
    }

    if (!cycle || cycle->is_depleted || cycle->current_capacity < harvest_amount)
    {
        return false; /* Cannot harvest */
    }

    cycle->current_capacity -= harvest_amount;
    cycle->last_harvest_time_us = get_current_time_us();

    if (cycle->current_capacity == 0)
    {
        cycle->is_depleted = true;
        /* Set regeneration time based on depletion */
        cycle->next_regeneration_time_us =
            cycle->last_harvest_time_us + (cycle->max_capacity * 100000); /* 0.1 sec per unit */
    }

    return true;
}

bool rogue_worldgen_resource_bridge_process_regeneration(RogueWorldGenResourceBridge* bridge)
{
    if (!bridge || !bridge->initialized)
        return false;

    uint64_t current_time = get_current_time_us();
    bool any_regenerated = false;

    for (uint32_t i = 0; i < bridge->depletion_cycle_count; i++)
    {
        ResourceDepletionCycle* cycle = &bridge->depletion_cycles[i];

        if (cycle->can_regenerate && current_time >= cycle->next_regeneration_time_us)
        {
            if (cycle->is_depleted && cycle->current_capacity < cycle->max_capacity)
            {
                /* Regenerate some capacity */
                uint32_t regen_amount = cycle->regeneration_rate;
                if (cycle->current_capacity + regen_amount > cycle->max_capacity)
                {
                    regen_amount = cycle->max_capacity - cycle->current_capacity;
                }

                cycle->current_capacity += regen_amount;
                cycle->next_regeneration_time_us =
                    current_time + 1000000; /* Next regen in 1 second */

                if (cycle->current_capacity >= cycle->max_capacity)
                {
                    cycle->is_depleted = false;
                }

                any_regenerated = true;
            }
        }
    }

    return any_regenerated;
}

/* 3.8.6: Rare Resource Events */

bool rogue_worldgen_resource_bridge_spawn_rare_event(RogueWorldGenResourceBridge* bridge,
                                                     RareResourceEventType event_type,
                                                     uint32_t region_id, float world_x,
                                                     float world_y)
{
    if (!bridge || !bridge->initialized || event_type >= RARE_EVENT_TYPE_COUNT || region_id >= 64 ||
        bridge->event_system.event_count >= MAX_RESOURCE_EVENTS)
    {
        return false;
    }

    RareResourceEvent* event = &bridge->event_system.events[bridge->event_system.event_count];
    event->event_id = bridge->event_system.event_count;
    event->event_type = event_type;
    event->region_id = region_id;
    event->world_x = world_x;
    event->world_y = world_y;

    /* Assign bonus resource based on event type */
    switch (event_type)
    {
    case RARE_EVENT_CRYSTAL_BLOOM:
        event->bonus_resource_type = RESOURCE_RARE_CRYSTALS;
        event->bonus_yield = random_uint32(50, 200);
        break;
    case RARE_EVENT_METAL_VEIN_DISCOVERY:
        event->bonus_resource_type = RESOURCE_METAL_ORE;
        event->bonus_yield = random_uint32(100, 300);
        break;
    case RARE_EVENT_ANCIENT_GROVE:
        event->bonus_resource_type = RESOURCE_WOOD;
        event->bonus_yield = random_uint32(80, 150);
        break;
    case RARE_EVENT_MAGICAL_SPRING:
        event->bonus_resource_type = RESOURCE_MAGICAL_ESSENCE;
        event->bonus_yield = random_uint32(30, 100);
        break;
    case RARE_EVENT_GEM_CLUSTER:
        event->bonus_resource_type = RESOURCE_GEMS;
        event->bonus_yield = random_uint32(20, 80);
        break;
    case RARE_EVENT_VOLCANIC_ERUPTION:
        event->bonus_resource_type = RESOURCE_STONE;
        event->bonus_yield = random_uint32(200, 500);
        break;
    default:
        event->bonus_resource_type = RESOURCE_STONE;
        event->bonus_yield = 50;
        break;
    }

    event->bonus_quality_multiplier = random_float(1.5f, 3.0f);
    event->event_start_time_us = get_current_time_us();
    event->event_duration_us = 1800000000ULL; /* 30 minutes */
    event->is_active = true;
    event->has_been_discovered = false;

    bridge->event_system.event_count++;
    bridge->event_system.active_event_count++;
    bridge->metrics.rare_events_spawned++;

    return true;
}

bool rogue_worldgen_resource_bridge_process_rare_events(RogueWorldGenResourceBridge* bridge)
{
    if (!bridge || !bridge->initialized || !bridge->event_system.events_enabled)
    {
        return false;
    }

    uint64_t current_time = get_current_time_us();
    bool events_processed = false;

    /* Check for event expiration */
    for (uint32_t i = 0; i < bridge->event_system.event_count; i++)
    {
        RareResourceEvent* event = &bridge->event_system.events[i];

        if (event->is_active &&
            current_time - event->event_start_time_us >= event->event_duration_us)
        {
            event->is_active = false;
            bridge->event_system.active_event_count--;
            events_processed = true;
        }
    }

    /* Spawn new events based on frequency */
    if (current_time - bridge->event_system.last_event_spawn_time_us > 10000000)
    { /* Check every 10 seconds */
        if (random_float(0.0f, 1.0f) <= bridge->event_system.global_event_frequency)
        {
            /* Spawn random event */
            RareResourceEventType event_type =
                (RareResourceEventType) random_uint32(0, RARE_EVENT_TYPE_COUNT - 1);
            uint32_t region_id = random_uint32(0, 63);
            float world_x = random_float(0.0f, 1000.0f);
            float world_y = random_float(0.0f, 1000.0f);

            rogue_worldgen_resource_bridge_spawn_rare_event(bridge, event_type, region_id, world_x,
                                                            world_y);
        }

        bridge->event_system.last_event_spawn_time_us = current_time;
    }

    return events_processed;
}

/* 3.8.7: Resource Discovery Mechanics */

bool rogue_worldgen_resource_bridge_add_discovery_location(RogueWorldGenResourceBridge* bridge,
                                                           float world_x, float world_y,
                                                           uint32_t region_id,
                                                           RogueResourceType hidden_resource,
                                                           uint32_t difficulty)
{
    if (!bridge || !bridge->initialized || region_id >= 64 ||
        hidden_resource >= RESOURCE_TYPE_COUNT ||
        bridge->discovery_system.location_count >= MAX_DISCOVERY_LOCATIONS)
    {
        return false;
    }

    ResourceDiscoveryLocation* location =
        &bridge->discovery_system.locations[bridge->discovery_system.location_count];
    location->location_id = bridge->discovery_system.location_count;
    location->world_x = world_x;
    location->world_y = world_y;
    location->region_id = region_id;
    location->hidden_resource_type = hidden_resource;
    location->discovery_difficulty = difficulty;
    location->discovery_radius = 50.0f; /* 50 unit radius */
    location->requires_tool = difficulty > 5;
    location->required_skill_level = (uint32_t) (difficulty * 10);
    location->has_been_discovered = false;
    location->discovery_time_us = 0;

    bridge->discovery_system.location_count++;
    return true;
}

bool rogue_worldgen_resource_bridge_attempt_discovery(RogueWorldGenResourceBridge* bridge,
                                                      float player_x, float player_y,
                                                      uint32_t player_skill_level,
                                                      uint32_t* out_discovery_id)
{
    if (!bridge || !bridge->initialized || !bridge->discovery_system.discovery_system_enabled ||
        !out_discovery_id)
    {
        return false;
    }

    *out_discovery_id = 0;

    /* Check all undiscovered locations within range */
    for (uint32_t i = 0; i < bridge->discovery_system.location_count; i++)
    {
        ResourceDiscoveryLocation* location = &bridge->discovery_system.locations[i];

        if (location->has_been_discovered)
            continue;

        float distance = distance_2d(player_x, player_y, location->world_x, location->world_y);
        if (distance <= location->discovery_radius)
        {
            /* Check skill requirements */
            if (player_skill_level < location->required_skill_level)
            {
                continue; /* Not skilled enough */
            }

            /* Calculate discovery success chance */
            float base_success = bridge->discovery_system.discovery_success_rate;
            float skill_bonus =
                (float) (player_skill_level - location->required_skill_level) / 100.0f;
            float difficulty_penalty = (float) location->discovery_difficulty / 20.0f;
            float success_chance = base_success + skill_bonus - difficulty_penalty;

            /* Clamp success chance */
            if (success_chance < 0.1f)
                success_chance = 0.1f;
            if (success_chance > 0.95f)
                success_chance = 0.95f;

            if (random_float(0.0f, 1.0f) <= success_chance)
            {
                /* Discovery successful! */
                location->has_been_discovered = true;
                location->discovery_time_us = get_current_time_us();
                bridge->discovery_system.discovered_count++;
                bridge->metrics.discoveries_made++;
                *out_discovery_id = location->location_id;
                return true;
            }
        }
    }

    return false; /* No discoveries made */
}

/* Utility Functions */

WorldGenResourceBridgeMetrics
rogue_worldgen_resource_bridge_get_metrics(const RogueWorldGenResourceBridge* bridge)
{
    if (!bridge || !bridge->initialized)
    {
        WorldGenResourceBridgeMetrics empty_metrics;
        memset(&empty_metrics, 0, sizeof(empty_metrics));
        return empty_metrics;
    }

    return bridge->metrics;
}

bool rogue_worldgen_resource_bridge_is_operational(const RogueWorldGenResourceBridge* bridge)
{
    if (!bridge)
        return false;

    return bridge->initialized && bridge->enabled;
}

uint32_t rogue_worldgen_resource_bridge_get_nodes_in_radius(RogueWorldGenResourceBridge* bridge,
                                                            float center_x, float center_y,
                                                            float radius, uint32_t* out_node_ids,
                                                            uint32_t max_nodes)
{
    if (!bridge || !bridge->initialized || !out_node_ids || max_nodes == 0)
    {
        return 0;
    }

    uint32_t found_nodes = 0;

    for (uint32_t i = 0; i < bridge->total_node_count && found_nodes < max_nodes; i++)
    {
        ResourceNode* node = &bridge->resource_nodes[i];

        if (!node->is_active)
            continue;

        float distance = distance_2d(center_x, center_y, node->world_x, node->world_y);
        if (distance <= radius)
        {
            out_node_ids[found_nodes] = node->node_id;
            found_nodes++;
        }
    }

    return found_nodes;
}
