/**
 * @file world_gen_resources.c
 * @brief Handles resource node registration, generation, and placement for world generation.
 * @details This module implements Phase 9: Resource Nodes Implementation, managing resource
 * descriptors, generating clusters of resource nodes based on biomes, and handling upgrades.
 */

/* Phase 9: Resource Nodes Implementation */
#include "world_gen.h"
#include <stdlib.h>
#include <string.h>

/** @def MAX_RESOURCE_DESCS
 * @brief Maximum number of resource descriptors that can be registered.
 */
#define MAX_RESOURCE_DESCS 64

/// @brief Global array of registered resource node descriptors.
static RogueResourceNodeDesc g_resource_descs[MAX_RESOURCE_DESCS];
/// @brief Current count of registered resource node descriptors.
static int g_resource_desc_count = 0;

/**
 * @brief Registers a new resource node descriptor.
 * @param d Pointer to the descriptor to register.
 * @return The index of the registered descriptor, or -1 on failure.
 */
int rogue_resource_register(const RogueResourceNodeDesc* d)
{
    if (!d || d->yield_min < 0 || d->yield_max < d->yield_min ||
        g_resource_desc_count >= MAX_RESOURCE_DESCS)
        return -1;
    g_resource_descs[g_resource_desc_count] = *d;
    return g_resource_desc_count++;
}

/**
 * @brief Clears the resource descriptor registry.
 */
void rogue_resource_clear_registry(void) { g_resource_desc_count = 0; }

/**
 * @brief Gets the count of registered resource descriptors.
 * @return The number of registered descriptors.
 */
int rogue_resource_registry_count(void) { return g_resource_desc_count; }

/**
 * @brief Retrieves a resource descriptor by index.
 * @param idx The index of the descriptor.
 * @return Pointer to the descriptor, or NULL if invalid.
 */
static const RogueResourceNodeDesc* rogue_resource_get(int idx)
{
    if (idx < 0 || idx >= g_resource_desc_count)
        return NULL;
    return &g_resource_descs[idx];
}

/**
 * @brief Gets the biome bitmask for a given tile type.
 * @param t The tile type.
 * @return The biome bitmask.
 */
static int biome_bit_for_tile(int t)
{
    switch (t)
    {
    case ROGUE_TILE_GRASS:
        return 1 << ROGUE_BIOME_PLAINS;
    case ROGUE_TILE_FOREST:
        return 1 << ROGUE_BIOME_FOREST_BIOME;
    case ROGUE_TILE_MOUNTAIN:
        return 1 << ROGUE_BIOME_MOUNTAIN_BIOME;
    case ROGUE_TILE_SNOW:
        return 1 << ROGUE_BIOME_SNOW_BIOME;
    case ROGUE_TILE_SWAMP:
        return 1 << ROGUE_BIOME_SWAMP_BIOME;
    default:
        return 0;
    }
}

/**
 * @brief Generates a random unsigned 32-bit integer from the RNG channel.
 * @param ch Pointer to the RNG channel.
 * @return A random unsigned 32-bit integer.
 */
static unsigned int rand_u32(RogueRngChannel* ch) { return rogue_worldgen_rand_u32(ch); }

/**
 * @brief Generates resource node placements in clusters.
 * @param cfg Pointer to the world generation config.
 * @param ctx Pointer to the world generation context.
 * @param map Pointer to the tile map.
 * @param out_array Array to store the placements.
 * @param max_out Maximum number of placements to generate.
 * @param cluster_attempts Number of attempts to find a seed tile.
 * @param cluster_radius Radius of each cluster.
 * @param base_clusters Number of base clusters to generate.
 * @return The number of placements generated.
 */
int rogue_resource_generate(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx,
                            const RogueTileMap* map, RogueResourceNodePlacement* out_array,
                            int max_out, int cluster_attempts, int cluster_radius,
                            int base_clusters)
{
    if (!cfg || !ctx || !map || !out_array || max_out <= 0)
        return 0;
    if (cluster_attempts <= 0)
        cluster_attempts = 64;
    if (cluster_radius < 1)
        cluster_radius = 3;
    if (base_clusters < 1)
        base_clusters = 4;
    int placed = 0;
    for (int c = 0; c < base_clusters && placed < max_out; c++)
    {
        /* pick random seed tile with suitable biome */
        int attempt = 0;
        int sx = 0, sy = 0;
        unsigned char base_tile = 0;
        int base_biome_bit = 0;
        while (attempt < cluster_attempts)
        {
            int x = (int) (rand_u32(&ctx->micro_rng) % (unsigned int) map->width);
            int y = (int) (rand_u32(&ctx->micro_rng) % (unsigned int) map->height);
            unsigned char t = map->tiles[y * map->width + x];
            base_biome_bit = biome_bit_for_tile(t);
            if (base_biome_bit)
            {
                sx = x;
                sy = y;
                base_tile = t;
                break;
            }
            attempt++;
        }
        if (!base_biome_bit)
            continue;
        int nodes_in_cluster = 2 + (int) (rand_u32(&ctx->micro_rng) % 3u); /* 2-4 */
        for (int i = 0; i < nodes_in_cluster && placed < max_out; i++)
        {
            int ox = ((int) rand_u32(&ctx->micro_rng) % (2 * cluster_radius + 1)) - cluster_radius;
            int oy = ((int) rand_u32(&ctx->micro_rng) % (2 * cluster_radius + 1)) - cluster_radius;
            int x = sx + ox;
            int y = sy + oy;
            if (x < 0 || y < 0 || x >= map->width || y >= map->height)
                continue;
            unsigned char t = map->tiles[y * map->width + x];
            int bit = biome_bit_for_tile(t);
            if (!(bit & base_biome_bit))
                continue; /* keep cluster homogeneous */
            /* select descriptor matching biome */
            int candidates[64];
            int cand_count = 0;
            for (int d = 0; d < g_resource_desc_count; d++)
            {
                if (g_resource_descs[d].biome_mask & bit)
                    candidates[cand_count++] = d;
            }
            if (!cand_count)
                continue;
            unsigned int r = rand_u32(&ctx->micro_rng) % (unsigned int) cand_count;
            int di = candidates[r];
            const RogueResourceNodeDesc* desc = &g_resource_descs[di];
            int yield =
                desc->yield_min + (int) (rand_u32(&ctx->micro_rng) %
                                         (unsigned int) (desc->yield_max - desc->yield_min + 1));
            /* upgrade chance: simple rarity scaling 5%,10%,18% for rarity 0,1,2 */
            int upgrade = 0;
            unsigned int ur = rand_u32(&ctx->micro_rng) % 100u;
            int thresh = (desc->rarity == 0) ? 5 : (desc->rarity == 1 ? 10 : 18);
            if (ur < (unsigned int) thresh)
            {
                upgrade = 1;
                yield = (int) (yield * 1.5f);
            }
            out_array[placed++] = (RogueResourceNodePlacement){x, y, di, yield, upgrade};
        }
    }
    return placed;
}

/**
 * @brief Counts the number of upgraded resource nodes in an array.
 * @param nodes Pointer to the array of placements.
 * @param count Number of placements in the array.
 * @return The number of upgraded nodes.
 */
int rogue_resource_upgrade_count(const RogueResourceNodePlacement* nodes, int count)
{
    int c = 0;
    for (int i = 0; i < count; i++)
        if (nodes[i].upgraded)
            c++;
    return c;
}
