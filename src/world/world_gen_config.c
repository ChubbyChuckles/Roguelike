/**
 * @file world_gen_config.c
 * @brief Unified world generation configuration construction.
 * @details This module provides a centralized function to build world generation configurations,
 * replacing duplicated initializers and handling scaling and app parameter integration.
 */

/* MIT License
 * Unified world gen config construction (replaces duplicated giant initializers). */
#include "world_gen_config.h"
#include "../core/app/app_state.h" /* for g_app when use_app_params */

/**
 * @brief Builds a world generation configuration.
 * @param seed Random seed for world generation.
 * @param use_app_params Whether to use application parameters for configuration.
 * @param apply_scale Whether to apply scaling to dimensions and biome regions.
 * @return A fully initialized RogueWorldGenConfig structure.
 * @details Constructs a configuration with default values, optionally overriding with app
 * parameters and applying scaling for larger worlds. Handles both advanced terrain and legacy
 * settings.
 */
RogueWorldGenConfig rogue_world_gen_config_build(unsigned int seed, int use_app_params,
                                                 int apply_scale)
{
    RogueWorldGenConfig cfg;
    cfg.seed = seed;
    cfg.width = 80;
    cfg.height = 60;               /* base logical dimensions before optional scaling */
    cfg.biome_regions = 10;        /* base count; may be overridden by scaling */
    cfg.continent_count = 3;       /* default macro layout target */
    cfg.biome_seed_offset = 7919u; /* large prime offset for biome rng decorrelation */
    cfg.cave_iterations = 3;
    cfg.cave_fill_chance = 0.45;
    cfg.river_attempts = 2;
    cfg.small_island_max_size = 3;
    cfg.small_island_passes = 2;
    cfg.shore_fill_passes = 1;
    cfg.advanced_terrain = 1;
    if (use_app_params)
    {
        cfg.water_level = g_app.gen_water_level;
        cfg.noise_octaves = g_app.gen_noise_octaves;
        cfg.noise_gain = g_app.gen_noise_gain;
        cfg.noise_lacunarity = g_app.gen_noise_lacunarity;
        cfg.river_sources = g_app.gen_river_sources;
        cfg.river_max_length = g_app.gen_river_max_length;
        cfg.cave_mountain_elev_thresh = g_app.gen_cave_thresh;
    }
    else
    {
        /* Default baseline constants (match persistence defaults & previous hardcoded path) */
        cfg.water_level = 0.34;
        cfg.noise_octaves = 6;
        cfg.noise_gain = 0.48;
        cfg.noise_lacunarity = 2.05;
        cfg.river_sources = 10;
        cfg.river_max_length = 1200;
        cfg.cave_mountain_elev_thresh = 0.60;
    }
    if (apply_scale)
    {
        cfg.width *= 10;
        cfg.height *= 10;
        cfg.biome_regions = 1000; /* 10 * 100 legacy scaling */
    }
    return cfg;
}
