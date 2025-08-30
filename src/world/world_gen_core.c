/**
 * @file world_gen_core.c
 * @brief Core orchestrator for world generation pipeline.
 * @details This module coordinates the entire world generation process, managing the sequence
 * of terrain generation, feature placement, and post-processing passes.
 */

/* Orchestrator for world generation (split refactor) */
#include "world_gen_internal.h"

/**
 * @brief Generates a complete world map.
 * @param out_map Pointer to the output tile map to populate.
 * @param cfg Pointer to the world generation configuration.
 * @return true on success, false on failure.
 * @details Orchestrates the world generation pipeline including base terrain, biome assignment,
 * cave carving, river placement, erosion, and smoothing passes. Supports both legacy and
 * advanced terrain generation modes.
 */
bool rogue_world_generate(RogueTileMap* out_map, const RogueWorldGenConfig* cfg)
{
    if (!out_map || !cfg)
        return false;
    if (!rogue_tilemap_init(out_map, cfg->width, cfg->height))
        return false;
    rng_seed(cfg->seed);

    /* Base / biome / elevation phase */
    wg_generate_base(out_map, cfg);

    if (!cfg->advanced_terrain)
    {
        /* Legacy feature passes */
        wg_generate_caves(out_map, cfg);
        wg_carve_rivers(out_map, cfg);
        wg_apply_erosion(out_map);
    }
    else
    {
        /* Advanced rivers + cave refinement */
        wg_advanced_post(out_map, cfg);
    }

    /* Small-island smoothing passes */
    int passes = cfg->small_island_passes > 0 ? cfg->small_island_passes : 1;
    int max_sz = cfg->small_island_max_size > 0 ? cfg->small_island_max_size : 3;
    for (int p = 0; p < passes; p++)
    {
        wg_smooth_small_islands(out_map, max_sz, ROGUE_TILE_WATER, 0);
        wg_smooth_small_islands(out_map, max_sz, ROGUE_TILE_GRASS, 0);
        wg_smooth_small_islands(out_map, max_sz, ROGUE_TILE_FOREST, 0);
        wg_smooth_small_islands(out_map, max_sz, ROGUE_TILE_MOUNTAIN, 0);
        wg_smooth_small_islands(out_map, max_sz, -1, 0); /* catch-all */
    }

    /* Shore thickening */
    int shore_passes = cfg->shore_fill_passes > 0 ? cfg->shore_fill_passes : 0;
    for (int s = 0; s < shore_passes; s++)
        wg_thicken_shores(out_map);

    return true;
}
