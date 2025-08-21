/* Phase 6: Structures & Points of Interest implementation */
#include "world/world_gen.h"
#include <stdlib.h>
#include <string.h>

/* Simple static registry (could later be dynamic / data-driven) */
#define ROGUE_MAX_STRUCTURE_DESCS 32
static RogueStructureDesc g_structure_descs[ROGUE_MAX_STRUCTURE_DESCS];
static int g_structure_desc_count = 0;

int rogue_world_register_default_structures(void)
{
    if (g_structure_desc_count > 0)
        return g_structure_desc_count; /* already registered */
    /* Example baseline structures: small hut, watchtower, shrine */
    g_structure_descs[g_structure_desc_count++] = (RogueStructureDesc){
        "hut", 5, 4, (1u << ROGUE_BIOME_PLAINS) | (1u << ROGUE_BIOME_FOREST_BIOME), 1.0, 0, 2, 1};
    g_structure_descs[g_structure_desc_count++] =
        (RogueStructureDesc){"watchtower",
                             3,
                             6,
                             (1u << ROGUE_BIOME_PLAINS) | (1u << ROGUE_BIOME_FOREST_BIOME) |
                                 (1u << ROGUE_BIOME_MOUNTAIN_BIOME),
                             0.6,
                             1,
                             3,
                             0};
    g_structure_descs[g_structure_desc_count++] =
        (RogueStructureDesc){"shrine",
                             4,
                             4,
                             (1u << ROGUE_BIOME_SWAMP_BIOME) | (1u << ROGUE_BIOME_SNOW_BIOME) |
                                 (1u << ROGUE_BIOME_PLAINS),
                             0.4,
                             0,
                             3,
                             1};
    return g_structure_desc_count;
}

int rogue_world_structure_desc_count(void) { return g_structure_desc_count; }
const RogueStructureDesc* rogue_world_get_structure_desc(int index)
{
    if (index < 0 || index >= g_structure_desc_count)
        return NULL;
    return &g_structure_descs[index];
}
void rogue_world_clear_structure_registry(void) { g_structure_desc_count = 0; }

/* Helper: pseudo elevation heuristic reused from erosion (duplicate logic minimal for now) */
static unsigned char tile_elevation(unsigned char t)
{
    switch (t)
    {
    case ROGUE_TILE_MOUNTAIN:
        return 3;
    case ROGUE_TILE_FOREST:
    case ROGUE_TILE_CAVE_WALL:
        return 2;
    case ROGUE_TILE_GRASS:
    case ROGUE_TILE_CAVE_FLOOR:
    case ROGUE_TILE_SWAMP:
    case ROGUE_TILE_SNOW:
        return 1;
    default:
        return 0;
    }
}

/* Very small biome inference from tile type (mirrors classification heuristics) */
static int tile_to_biome(unsigned char t)
{
    switch (t)
    {
    case ROGUE_TILE_WATER:
        return ROGUE_BIOME_OCEAN;
    case ROGUE_TILE_GRASS:
        return ROGUE_BIOME_PLAINS;
    case ROGUE_TILE_FOREST:
        return ROGUE_BIOME_FOREST_BIOME;
    case ROGUE_TILE_MOUNTAIN:
        return ROGUE_BIOME_MOUNTAIN_BIOME;
    case ROGUE_TILE_SNOW:
        return ROGUE_BIOME_SNOW_BIOME;
    case ROGUE_TILE_SWAMP:
        return ROGUE_BIOME_SWAMP_BIOME;
    default:
        return ROGUE_BIOME_PLAINS;
    }
}

/* Poisson-ish rejection: sample random positions, enforce min spacing & constraints */
int rogue_world_place_structures(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx,
                                 RogueTileMap* io_map, RogueStructurePlacement* out_array,
                                 int max_out, int min_spacing)
{
    if (!cfg || !ctx || !io_map || !out_array || max_out <= 0)
        return 0;
    if (min_spacing < 2)
        min_spacing = 2;
    int desc_count = rogue_world_register_default_structures();
    int placed = 0;
    int attempts = max_out * 20;
    int w = io_map->width, h = io_map->height;
    while (attempts-- > 0 && placed < max_out)
    {
        /* pick descriptor weighted by rarity */
        double total_w = 0;
        for (int i = 0; i < desc_count; i++)
            total_w += g_structure_descs[i].rarity;
        double r = rogue_worldgen_rand_norm(&ctx->micro_rng) * total_w;
        int pick = 0;
        for (int i = 0; i < desc_count; i++)
        {
            r -= g_structure_descs[i].rarity;
            if (r <= 0)
            {
                pick = i;
                break;
            }
        }
        const RogueStructureDesc* desc = &g_structure_descs[pick];
        int rot = 0;
        int sw = desc->width, sh = desc->height;
        if (desc->allow_rotation && rogue_worldgen_rand_norm(&ctx->micro_rng) < 0.5)
        {
            rot = 1;
            sw = desc->height;
            sh = desc->width;
        }
        int x = (int) (rogue_worldgen_rand_norm(&ctx->micro_rng) * (w - sw - 2)) + 1;
        int y = (int) (rogue_worldgen_rand_norm(&ctx->micro_rng) * (h - sh - 2)) + 1;
        if (x < 1 || y < 1 || x + sw >= w || y + sh >= h)
            continue;
        /* spacing constraint */
        int too_close = 0;
        for (int i = 0; i < placed; i++)
        {
            int dx = out_array[i].x - x;
            int dy = out_array[i].y - y;
            if (dx < 0)
                dx = -dx;
            if (dy < 0)
                dy = -dy;
            if (dx < (out_array[i].w + sw) / 2 + min_spacing &&
                dy < (out_array[i].h + sh) / 2 + min_spacing)
            {
                too_close = 1;
                break;
            }
        }
        if (too_close)
            continue;
        /* biome/elevation fitness */
        int bx = x + sw / 2;
        int by = y + sh / 2;
        unsigned char ct = io_map->tiles[by * w + bx];
        int biome = tile_to_biome(ct);
        unsigned char elev = tile_elevation(ct);
        if (((1u << biome) & desc->biome_mask) == 0)
            continue;
        if (elev < desc->min_elevation || elev > desc->max_elevation)
            continue;
        /* occupancy check (avoid water & mountains for footprint interior) */
        int blocked = 0;
        for (int yy = 0; yy < sh && !blocked; yy++)
            for (int xx = 0; xx < sw; xx++)
            {
                unsigned char t = io_map->tiles[(y + yy) * w + (x + xx)];
                if (t == ROGUE_TILE_WATER || t == ROGUE_TILE_MOUNTAIN || t == ROGUE_TILE_RIVER ||
                    t == ROGUE_TILE_RIVER_WIDE)
                {
                    blocked = 1;
                    break;
                }
            }
        if (blocked)
            continue;
        /* carve structure: border walls + interior floors */
        for (int yy = 0; yy < sh; yy++)
            for (int xx = 0; xx < sw; xx++)
            {
                int idx = (y + yy) * w + (x + xx);
                int border = (yy == 0 || yy == sh - 1 || xx == 0 || xx == sw - 1);
                io_map->tiles[idx] = (unsigned char) (border ? ROGUE_TILE_STRUCTURE_WALL
                                                             : ROGUE_TILE_STRUCTURE_FLOOR);
            }
        out_array[placed++] = (RogueStructurePlacement){x, y, sw, sh, rot, desc};
    }
    return placed;
}

int rogue_world_place_dungeon_entrances(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx,
                                        RogueTileMap* io_map,
                                        const RogueStructurePlacement* placements,
                                        int placement_count, int max_entrances)
{
    (void) cfg;
    if (!ctx || !io_map || !placements || placement_count <= 0 || max_entrances <= 0)
        return 0;
    int w = io_map->width, h = io_map->height;
    if (max_entrances > placement_count)
        max_entrances = placement_count;
    int placed = 0; /* choose subset deterministically */
    for (int i = 0; i < placement_count && placed < max_entrances; i++)
    {
        /* small probability skip to diversify */
        if (rogue_worldgen_rand_norm(&ctx->micro_rng) < 0.25)
            continue;
        const RogueStructurePlacement* sp = &placements[i];
        int cx = sp->x + sp->w / 2;
        int cy = sp->y + sp->h;
        if (cy + 1 >= h)
            continue; /* place just below structure */
        unsigned char below = io_map->tiles[(cy + 0) * w + cx];
        if (below == ROGUE_TILE_STRUCTURE_FLOOR)
        {
            io_map->tiles[(cy + 0) * w + cx] = ROGUE_TILE_DUNGEON_ENTRANCE;
            placed++;
        }
    }
    return placed;
}
