#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void analyze_map(const RogueTileMap* map, int* water, int* land)
{
    *water = *land = 0;
    for (int y = 0; y < map->height; y++)
        for (int x = 0; x < map->width; x++)
        {
            unsigned char t = map->tiles[y * map->width + x];
            if (t == ROGUE_TILE_WATER)
                (*water)++;
            else if (t != ROGUE_TILE_RIVER)
                (*land)++;
        }
}

int main(void)
{
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(424242u, 0, 0);
    cfg.width = 128;
    cfg.height = 96;
    cfg.river_sources = 12;
    cfg.river_max_length = 800;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueTileMap map;
    int hist[ROGUE_TILE_MAX];
    int continents = 0;
    memset(hist, 0, sizeof hist);
    if (!rogue_world_generate_macro_layout(&cfg, &ctx, &map, hist, &continents))
    {
        fprintf(stderr, "ERROR: macro layout generation failed early\n");
        return 2;
    }
    int water = 0, land = 0;
    analyze_map(&map, &water, &land);
    int total = map.width * map.height;
    printf("macro phase: w=%d h=%d water=%d land=%d continents=%d rivers=%d\n", map.width,
           map.height, water, land, continents, hist[ROGUE_TILE_RIVER]);
    if (total == 0)
    {
        fprintf(stderr, "ERROR: map has zero area (%d x %d)\n", map.width, map.height);
        return 3;
    }
    if (!(water > total * 0.10 && water < total * 0.70))
    {
        fprintf(stderr, "ERROR: water ratio out of bounds: water=%d total=%d\n", water, total);
        return 4;
    }
    if (continents < 1)
    {
        fprintf(stderr, "ERROR: continent count <1 (%d)\n", continents);
        return 5;
    }
    unsigned long long h1 = rogue_world_hash_tilemap(&map);
    int hist_copy[ROGUE_TILE_MAX];
    memcpy(hist_copy, hist, sizeof hist_copy);
    rogue_tilemap_free(&map);
    /* Reinitialize context so RNG channel state resets for deterministic comparison */
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueTileMap map2;
    if (!rogue_world_generate_macro_layout(&cfg, &ctx, &map2, hist, &continents))
    {
        fprintf(stderr, "ERROR: second macro layout generation failed\n");
        return 6;
    }
    unsigned long long h2 = rogue_world_hash_tilemap(&map2);
    if (h1 != h2)
    {
        fprintf(stderr, "ERROR: hash mismatch %llu vs %llu\n", h1, h2);
        return 7;
    }
    if (memcmp(hist_copy, hist, sizeof hist) != 0)
    {
        fprintf(stderr, "ERROR: histogram mismatch on regen\n");
        return 8;
    }
    int river_tiles = 0, river_adj_ocean = 0;
    for (int y = 1; y < map2.height - 1; y++)
        for (int x = 1; x < map2.width - 1; x++)
        {
            unsigned char t = map2.tiles[y * map2.width + x];
            if (t == ROGUE_TILE_RIVER)
            {
                river_tiles++;
                for (int oy = -1; oy <= 1; oy++)
                    for (int ox = -1; ox <= 1; ox++)
                    {
                        if (!ox && !oy)
                            continue;
                        unsigned char nt = map2.tiles[(y + oy) * map2.width + (x + ox)];
                        if (nt == ROGUE_TILE_WATER)
                        {
                            river_adj_ocean++;
                            goto done_adj;
                        }
                    }
            }
        done_adj:;
        }
    if (!(river_tiles > 0 && river_adj_ocean > 0))
    {
        fprintf(stderr, "ERROR: river validation failed tiles=%d adj_ocean=%d\n", river_tiles,
                river_adj_ocean);
        return 9;
    }
    rogue_tilemap_free(&map2);
    printf("phase2 macro tests passed\n");
    return 0;
}
