/**
 * @file world_gen_rivers.c
 * @brief Handles river refinement, erosion simulation, and bridge hint marking for world
 * generation.
 * @details This module implements Phase 5: River refinement & erosion detailing, including widening
 * rivers, applying thermal and hydraulic erosion, computing steepness metrics, and identifying
 * bridge locations.
 */

/* Phase 5: River refinement & erosion detailing */
#include "world_gen.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

double fbm(double x, double y, int octaves, double lacunarity, double gain);

/**
 * @brief Generates a normalized random double from the RNG channel.
 * @param ch Pointer to the RNG channel.
 * @return A random double between 0 and 1.
 */
static double prand(RogueRngChannel* ch) { return rogue_worldgen_rand_norm(ch); }

/**
 * @brief Generates a random integer within a specified range.
 * @param ch Pointer to the RNG channel.
 * @param lo Lower bound (inclusive).
 * @param hi Upper bound (inclusive).
 * @return A random integer in [lo, hi].
 */
static int prand_range(RogueRngChannel* ch, int lo, int hi)
{
    if (hi <= lo)
        return lo;
    int span = hi - lo + 1;
    int v = lo + (int) (prand(ch) * span);
    if (v > hi)
        v = hi;
    return v;
}

/**
 * @brief Refines rivers by widening tiles based on noise and converting deltas.
 * @param cfg Pointer to the world generation config.
 * @param ctx Pointer to the world generation context.
 * @param io_map Pointer to the tile map to modify.
 * @return True if successful, false otherwise.
 */
bool rogue_world_refine_rivers(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx,
                               RogueTileMap* io_map)
{
    if (!cfg || !ctx || !io_map)
        return false;
    int w = io_map->width,
        h = io_map->height; /* widen some river tiles based on noise & meander smoothing */
    unsigned char* copy = (unsigned char*) malloc((size_t) w * h);
    if (!copy)
        return false;
    memcpy(copy, io_map->tiles, (size_t) w * h);
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            int idx = y * w + x;
            if (copy[idx] == ROGUE_TILE_RIVER)
            {
                double n = fbm(x * 0.12 + 7, y * 0.12 + 11, 3, 2.0, 0.5);
                if (n > 0.35)
                { /* widen cross */
                    for (int oy = -1; oy <= 1; oy++)
                        for (int ox = -1; ox <= 1; ox++)
                        {
                            int nx = x + ox, ny = y + oy;
                            if (nx < 0 || ny < 0 || nx >= w || ny >= h)
                                continue;
                            if (io_map->tiles[ny * w + nx] == ROGUE_TILE_WATER)
                                io_map->tiles[ny * w + nx] = ROGUE_TILE_RIVER_WIDE;
                        }
                }
            }
        }
    /* Convert isolated wide tiles adjacent to >=4 water tiles into delta markers */
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            int idx = y * w + x;
            if (io_map->tiles[idx] == ROGUE_TILE_RIVER_WIDE)
            {
                int water = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        unsigned char t = io_map->tiles[(y + oy) * w + (x + ox)];
                        if (t == ROGUE_TILE_WATER)
                            water++;
                    }
                if (water >= 4)
                    io_map->tiles[idx] = ROGUE_TILE_RIVER_DELTA;
            }
        }
    free(copy);
    return true;
}

/**
 * @brief Applies thermal and hydraulic erosion to the tile map.
 * @param cfg Pointer to the world generation config.
 * @param ctx Pointer to the world generation context.
 * @param io_map Pointer to the tile map to modify.
 * @param thermal_passes Number of thermal erosion passes.
 * @param hydraulic_passes Number of hydraulic erosion passes.
 * @return True if successful, false otherwise.
 */
bool rogue_world_apply_erosion(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx,
                               RogueTileMap* io_map, int thermal_passes, int hydraulic_passes)
{
    (void) cfg;
    if (!ctx || !io_map)
        return false;
    int w = io_map->width, h = io_map->height;
    if (thermal_passes < 0)
        thermal_passes = 0;
    if (hydraulic_passes < 0)
        hydraulic_passes = 0;
    /* Represent elevation heuristically: mountain=3, forest=2, grass=1, water/river=0, cave wall=2,
     * cave floor=1 */
    int count = w * h;
    unsigned char* elev = (unsigned char*) malloc((size_t) count);
    if (!elev)
        return false;
    for (int i = 0; i < count; i++)
    {
        unsigned char t = io_map->tiles[i];
        unsigned char e = 0;
        switch (t)
        {
        case ROGUE_TILE_MOUNTAIN:
            e = 3;
            break;
        case ROGUE_TILE_FOREST:
        case ROGUE_TILE_CAVE_WALL:
            e = 2;
            break;
        case ROGUE_TILE_GRASS:
        case ROGUE_TILE_CAVE_FLOOR:
        case ROGUE_TILE_SWAMP:
        case ROGUE_TILE_SNOW:
            e = 1;
            break;
        default:
            e = 0;
            break;
        }
        elev[i] = e;
    }
    /* Thermal: if a high cell has >=3 lower neighbors, reduce by one (simulating creep) */
    for (int pass = 0; pass < thermal_passes; ++pass)
    {
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                unsigned char e = elev[idx];
                if (e <= 1)
                    continue;
                int lower = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        unsigned char ne = elev[(y + oy) * w + (x + ox)];
                        if (ne < e)
                            lower++;
                    }
                if (lower >= 3 && prand(&ctx->macro_rng) < 0.35)
                    elev[idx]--;
            }
    }
    /* Hydraulic: randomly lower steep pairs and mark adjacent river tiles for widening */
    for (int pass = 0; pass < hydraulic_passes; ++pass)
    {
        for (int y = 1; y < h - 1; y++)
            for (int x = 1; x < w - 1; x++)
            {
                int idx = y * w + x;
                unsigned char e = elev[idx];
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        int nx = x + ox, ny = y + oy;
                        int nidx = ny * w + nx;
                        unsigned char ne = elev[nidx];
                        if (e > ne + 1 && prand(&ctx->macro_rng) < 0.20)
                        {
                            elev[idx]--;
                            if (io_map->tiles[idx] == ROGUE_TILE_RIVER)
                                io_map->tiles[idx] = ROGUE_TILE_RIVER_WIDE;
                        }
                    }
            }
    }
    /* Apply smoothing back to tiles: lower mountains -> forest/grass */
    for (int i = 0; i < count; i++)
    {
        unsigned char t = io_map->tiles[i];
        unsigned char e = elev[i];
        if (t == ROGUE_TILE_MOUNTAIN && e < 3)
        {
            io_map->tiles[i] = (e >= 2) ? ROGUE_TILE_FOREST : ROGUE_TILE_GRASS;
        }
    }
    free(elev);
    return true;
}

/**
 * @brief Computes the average steepness metric between two tile maps.
 * @param before Pointer to the before tile map.
 * @param after Pointer to the after tile map.
 * @return The average steepness metric, or 0.0 if invalid.
 */
double rogue_world_compute_steepness_metric(const RogueTileMap* before, const RogueTileMap* after)
{
    if (!before || !after || before->width != after->width || before->height != after->height)
        return 0.0;
    int w = before->width, h = before->height;
    double sum = 0;
    int samples = 0;
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++)
        {
            int idx = y * w + x;
            unsigned char tb = before->tiles[idx];
            unsigned char ta = after->tiles[idx];
            if (tb == ta)
                continue;
            samples++;
            int diff = (int) tb - (int) ta;
            if (diff < 0)
                diff = -diff;
            sum += diff;
        }
    return samples ? (sum / samples) : 0.0;
}

/**
 * @brief Marks potential bridge hints by counting suitable water gaps.
 * @param cfg Pointer to the world generation config.
 * @param map Pointer to the tile map.
 * @param min_gap Minimum gap size for a bridge.
 * @param max_gap Maximum gap size for a bridge.
 * @return The number of potential bridge locations found.
 */
int rogue_world_mark_bridge_hints(const RogueWorldGenConfig* cfg, const RogueTileMap* map,
                                  int min_gap, int max_gap)
{
    (void) cfg;
    if (!map)
        return 0;
    int w = map->width, h = map->height;
    if (min_gap < 2)
        min_gap = 2;
    if (max_gap < min_gap)
        max_gap = min_gap;
    int marked = 0; /* Scan horizontal water gaps between land */
    for (int y = 1; y < h - 1; y++)
    {
        int x = 0;
        while (x < w)
        {
            while (x < w && map->tiles[y * w + x] != ROGUE_TILE_WATER)
                x++;
            int start = x;
            while (x < w && map->tiles[y * w + x] == ROGUE_TILE_WATER)
                x++;
            int end = x - 1;
            if (start > 0 && end < w - 1)
            {
                int left = start - 1, right = end + 1;
                unsigned char lt = map->tiles[y * w + left];
                unsigned char rt = map->tiles[y * w + right];
                if (lt != ROGUE_TILE_WATER && rt != ROGUE_TILE_WATER)
                {
                    int gap = end - start + 1;
                    if (gap >= min_gap && gap <= max_gap)
                    {
                        /* Count potential bridge placement; intentionally not mutating map (const)
                         */
                        marked++;
                    }
                }
            }
        }
    }
    return marked;
}
