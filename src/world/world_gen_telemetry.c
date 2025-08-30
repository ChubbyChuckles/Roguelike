/**
 * @file world_gen_telemetry.c
 * @brief Handles telemetry and metrics collection for world generation.
 *
 * This module provides functions to collect metrics from generated tile maps,
 * detect anomalies, and export data for analysis. Phase 12: Telemetry & Analytics Implementation
 */

#include "tilemap.h"
#include "world_gen.h"
#include <string.h>

/**
 * @brief Collects metrics from a tile map and detects anomalies.
 *
 * @param map Pointer to the tile map to analyze.
 * @param out_m Pointer to the output metrics structure.
 * @return 1 on success, 0 on failure.
 */
int rogue_world_metrics_collect(const RogueTileMap* map, RogueWorldGenMetrics* out_m)
{
    if (!map || !out_m)
        return 0;
    memset(out_m, 0, sizeof *out_m);
    int w = map->width, h = map->height;
    if (w <= 0 || h <= 0)
        return 0;
    int land = 0, water = 0, river = 0;
    for (int i = 0; i < w * h; i++)
    {
        unsigned char t = map->tiles[i];
        switch (t)
        {
        case ROGUE_TILE_WATER:
            water++;
            break;
        case ROGUE_TILE_RIVER:
        case ROGUE_TILE_RIVER_WIDE:
        case ROGUE_TILE_RIVER_DELTA:
            river++;
            break;
        default:
            land++;
            break;
        }
    }
    double land_ratio = (land + river) / (double) (land + river + water + 1e-6);
    if (land_ratio < 0.30 || land_ratio > 0.55)
        out_m->anomalies |= 0x1; /* land ratio out-of-bounds */
    if (river == 0)
        out_m->anomalies |= 0x2; /* missing rivers */
    out_m->continents =
        1; /* placeholder: counting continents would replicate macro algorithm; omitted for cost */
    out_m->rivers = river; /* number of river tiles */
    return 1;
}

/**
 * @brief Helper function to copy a token into a buffer with comma separation.
 *
 * @param dst Destination buffer.
 * @param cap Capacity of the buffer.
 * @param pos Current position in the buffer.
 * @param first Flag indicating if this is the first token.
 * @param token Token to copy.
 */
static void copy_token(char* dst, size_t cap, size_t* pos, int* first, const char* token)
{
    if (!dst || !token)
        return;
    size_t len = strlen(token);
    if (!*first)
    {
        if (*pos + 2 < cap)
        {
            dst[(*pos)++] = ',';
            dst[(*pos)++] = ' ';
        }
    }
    if (*pos + len < cap)
    {
        for (size_t i = 0; i < len; i++)
        {
            dst[*pos + i] = token[i];
        }
        *pos += len;
        dst[*pos] = '\0';
    }
    *first = 0;
}

/**
 * @brief Generates a comma-separated list of anomaly names from metrics.
 *
 * @param m Pointer to the metrics structure.
 * @param buf Output buffer for the list.
 * @param cap Capacity of the output buffer.
 */
void rogue_world_metrics_anomaly_list(const RogueWorldGenMetrics* m, char* buf, size_t cap)
{
    if (!buf || cap == 0)
    {
        return;
    }
    buf[0] = '\0';
    if (!m)
        return;
    int first = 1;
    size_t pos = 0;
    if (m->anomalies & 0x1)
    {
        copy_token(buf, cap, &pos, &first, "land_ratio");
    }
    if (m->anomalies & 0x2)
    {
        copy_token(buf, cap, &pos, &first, "no_rivers");
    }
}

/**
 * @brief Exports the biome heatmap from a tile map.
 *
 * @param map Pointer to the tile map.
 * @param out Output buffer for the heatmap data.
 * @param cap Capacity of the output buffer.
 * @return 1 on success, 0 on failure.
 */
int rogue_world_export_biome_heatmap(const RogueTileMap* map, unsigned char* out, size_t cap)
{
    if (!map || !out)
        return 0;
    size_t need = (size_t) map->width * map->height;
    if (cap < need)
        return 0;
    memcpy(out, map->tiles, need);
    return 1;
}
