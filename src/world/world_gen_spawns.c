/**
 * @file world_gen_spawns.c
 * @brief Handles spawn table registration, density map building, and sampling for world generation.
 * @details This module implements Phase 8: Fauna & Spawn Ecology, managing spawn tables for
 * different biomes, building density maps based on tile types, and sampling spawns with rarity
 * considerations.
 */

/* Phase 8: Fauna & Spawn Ecology Implementation */
#include "world_gen.h"
#include <stdlib.h>
#include <string.h>

/** @def MAX_SPAWN_TABLES
 * @brief Maximum number of spawn tables that can be registered.
 */
#define MAX_SPAWN_TABLES 32

/// @brief Global array of registered spawn tables.
static RogueSpawnTable g_spawn_tables[MAX_SPAWN_TABLES];
/// @brief Current count of registered spawn tables.
static int g_spawn_table_count = 0;

/**
 * @brief Clears all registered spawn tables.
 */
void rogue_spawn_clear_tables(void) { g_spawn_table_count = 0; }

/**
 * @brief Registers a new spawn table.
 * @param table Pointer to the spawn table to register.
 * @return The index of the registered table, or -1 on failure.
 */
int rogue_spawn_register_table(const RogueSpawnTable* table)
{
    if (!table || table->entry_count <= 0 || table->entry_count > 16)
        return -1;
    if (g_spawn_table_count >= MAX_SPAWN_TABLES)
        return -1;
    g_spawn_tables[g_spawn_table_count] = *table;
    return g_spawn_table_count++;
}

/**
 * @brief Retrieves the spawn table for a given tile type.
 * @param tile_type The tile type to look up.
 * @return Pointer to the spawn table, or NULL if not found.
 */
const RogueSpawnTable* rogue_spawn_get_table_for_tile(int tile_type)
{
    for (int i = 0; i < g_spawn_table_count; i++)
    {
        if (g_spawn_tables[i].biome_tile == tile_type)
            return &g_spawn_tables[i];
    }
    return NULL;
}

/**
 * @brief Builds a spawn density map based on the tile map.
 * @param map Pointer to the tile map.
 * @param out_dm Pointer to the output density map.
 * @return True if successful, false otherwise.
 */
bool rogue_spawn_build_density(const RogueTileMap* map, RogueSpawnDensityMap* out_dm)
{
    if (!map || !out_dm)
        return false;
    int w = map->width, h = map->height;
    size_t count = (size_t) w * h;
    out_dm->width = w;
    out_dm->height = h;
    out_dm->density = (float*) malloc(sizeof(float) * count);
    if (!out_dm->density)
        return false;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int idx = y * w + x;
            unsigned char t = map->tiles[idx];
            float base = 0.0f;
            switch (t)
            {
            case ROGUE_TILE_GRASS:
                base = 0.6f;
                break;
            case ROGUE_TILE_FOREST:
                base = 0.9f;
                break;
            case ROGUE_TILE_SWAMP:
                base = 0.4f;
                break;
            case ROGUE_TILE_SNOW:
                base = 0.35f;
                break;
            case ROGUE_TILE_DUNGEON_FLOOR:
                base = 0.5f;
                break;
            default:
                base = 0.0f;
                break;
            }
            /* Water proximity dampening */
            if (base > 0)
            {
                int water_adj = 0;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                        if (ox || oy)
                        {
                            int nx = x + ox, ny = y + oy;
                            if (nx >= 0 && ny >= 0 && nx < w && ny < h)
                            {
                                unsigned char nt = map->tiles[ny * w + nx];
                                if (nt == ROGUE_TILE_WATER || nt == ROGUE_TILE_RIVER ||
                                    nt == ROGUE_TILE_RIVER_WIDE)
                                    water_adj++;
                            }
                        }
                if (water_adj >= 3)
                    base *= 0.35f;
                else if (water_adj >= 1)
                    base *= 0.7f;
            }
            out_dm->density[idx] = base;
        }
    return true;
}

/**
 * @brief Frees the memory allocated for the spawn density map.
 * @param dm Pointer to the density map to free.
 */
void rogue_spawn_free_density(RogueSpawnDensityMap* dm)
{
    if (!dm)
        return;
    free(dm->density);
    dm->density = NULL;
}

/**
 * @brief Applies hub suppression to the spawn density map.
 * @param dm Pointer to the density map.
 * @param hub_x X-coordinate of the hub.
 * @param hub_y Y-coordinate of the hub.
 * @param radius Suppression radius.
 */
void rogue_spawn_apply_hub_suppression(RogueSpawnDensityMap* dm, int hub_x, int hub_y, int radius)
{
    if (!dm || !dm->density || radius <= 0)
        return;
    int w = dm->width, h = dm->height;
    float r2 = (float) (radius * radius);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
        {
            int dx = x - hub_x, dy = y - hub_y;
            int d2 = dx * dx + dy * dy;
            float* val = &dm->density[y * w + x];
            if (d2 <= radius * radius)
            {
                *val = 0.0f;
            }
            else if (d2 < (int) (r2 * 1.44f))
            {
                float t = (float) d2 / r2 - 1.0f;
                if (t < 0)
                    t = 0;
                if (t > 1)
                    t = 1;
                *val *= t;
            }
        }
}

/**
 * @brief Chooses a weighted random entry from the spawn entries.
 * @param ch Pointer to the RNG channel.
 * @param entries Array of spawn entries.
 * @param count Number of entries.
 * @param rare Whether to use rare weights.
 * @return Index of the chosen entry, or -1 if none.
 */
static int choose_weighted(RogueRngChannel* ch, const RogueSpawnEntry* entries, int count, int rare)
{
    int total = 0;
    for (int i = 0; i < count; i++)
        total += rare ? entries[i].rare_weight : entries[i].weight;
    if (total <= 0)
        return -1;
    unsigned int r = rogue_worldgen_rand_u32(ch) % (unsigned int) total;
    int accum = 0;
    for (int i = 0; i < count; i++)
    {
        int w = rare ? entries[i].rare_weight : entries[i].weight;
        if (w <= 0)
            continue;
        if (r < (unsigned int) (accum + w))
            return i;
        accum += w;
    }
    return -1;
}

/**
 * @brief Samples a spawn at the given position.
 * @param ctx Pointer to the world generation context.
 * @param dm Pointer to the density map.
 * @param map Pointer to the tile map.
 * @param x X-coordinate.
 * @param y Y-coordinate.
 * @param out_id Buffer to store the spawn ID.
 * @param id_cap Capacity of the out_id buffer.
 * @param out_is_rare Pointer to store whether the spawn is rare.
 * @return 1 if a spawn was sampled, 0 otherwise.
 */
int rogue_spawn_sample(RogueWorldGenContext* ctx, const RogueSpawnDensityMap* dm,
                       const RogueTileMap* map, int x, int y, char* out_id, size_t id_cap,
                       int* out_is_rare)
{
    if (out_is_rare)
        *out_is_rare = 0;
    if (!ctx || !dm || !map || !out_id)
        return 0;
    if (x < 0 || y < 0 || x >= map->width || y >= map->height)
        return 0;
    int idx = y * map->width + x;
    float density = dm->density ? dm->density[idx] : 0.0f;
    if (density <= 0.01f)
        return 0;
    unsigned char tile = map->tiles[idx];
    const RogueSpawnTable* table = rogue_spawn_get_table_for_tile(tile);
    if (!table)
        return 0;
    int rare = 0;
    if (table->rare_chance_bp > 0)
    {
        unsigned int r = rogue_worldgen_rand_u32(&ctx->micro_rng) % 10000u;
        if (r < (unsigned int) table->rare_chance_bp)
            rare = 1;
    }
    int chosen = choose_weighted(&ctx->micro_rng, table->entries, table->entry_count, rare);
    if (chosen < 0)
        return 0;
    if (out_is_rare)
        *out_is_rare = rare;
    if (id_cap)
    {
        size_t src_len = 0;
        while (src_len < sizeof(table->entries[chosen].id) && table->entries[chosen].id[src_len])
            src_len++;
        size_t copy = (src_len < id_cap - 1) ? src_len : (id_cap - 1);
        for (size_t i = 0; i < copy; i++)
            out_id[i] = table->entries[chosen].id[i];
        out_id[copy] = '\0';
    }
    return 1;
}
