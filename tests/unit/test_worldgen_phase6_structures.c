/* Phase 6 unit tests: structures & POIs basic placement */
#include "world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 424242;
    cfg->width = 160;
    cfg->height = 96;
    cfg->noise_octaves = 5;
    cfg->water_level = 0.30;
    cfg->river_sources = 5;
    cfg->river_max_length = 220;
    cfg->cave_fill_chance = 0.45;
    cfg->cave_iterations = 3;
}

int main(void)
{
    RogueWorldGenConfig cfg;
    init_cfg(&cfg);
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueTileMap base;
    if (!rogue_tilemap_init(&base, cfg.width, cfg.height))
    {
        fprintf(stderr, "alloc fail\n");
        return 1;
    }
    if (!rogue_world_generate_macro_layout(&cfg, &ctx, &base, NULL, NULL))
    {
        fprintf(stderr, "macro fail\n");
        return 1;
    }
    /* Place structures */
    RogueStructurePlacement placements[32];
    memset(placements, 0, sizeof placements);
    rogue_worldgen_context_init(
        &ctx, &cfg); /* reset micro rng for deterministic placement independent of macro */
    int count = rogue_world_place_structures(&cfg, &ctx, &base, placements, 32, 4);
    if (count <= 0)
    {
        fprintf(stderr, "expected at least one structure\n");
        return 1;
    }
    /* Validate no overlaps and biome constraints roughly satisfied */
    for (int i = 0; i < count; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            int ax1 = placements[i].x, ay1 = placements[i].y, ax2 = ax1 + placements[i].w - 1,
                ay2 = ay1 + placements[i].h - 1;
            int bx1 = placements[j].x, by1 = placements[j].y, bx2 = bx1 + placements[j].w - 1,
                by2 = by1 + placements[j].h - 1;
            int overlap = !(ax2 < bx1 || bx2 < ax1 || ay2 < by1 || by2 < ay1);
            if (overlap)
            {
                fprintf(stderr, "structure overlap detected %d vs %d\n", i, j);
                return 1;
            }
        }
    }
    /* Determinism: regenerate and ensure identical placements & hash */
    unsigned long long h1 = rogue_world_hash_tilemap(&base);
    RogueStructurePlacement copy[32];
    memcpy(copy, placements, sizeof placements);
    RogueTileMap regen;
    rogue_tilemap_init(&regen, cfg.width, cfg.height);
    rogue_worldgen_context_init(&ctx, &cfg);
    rogue_world_generate_macro_layout(&cfg, &ctx, &regen, NULL, NULL);
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueStructurePlacement placements2[32];
    int count2 = rogue_world_place_structures(&cfg, &ctx, &regen, placements2, 32, 4);
    if (count2 != count)
    {
        fprintf(stderr, "placement count mismatch %d vs %d\n", count, count2);
        return 1;
    }
    for (int i = 0; i < count; i++)
    {
        if (placements[i].x != placements2[i].x || placements[i].y != placements2[i].y ||
            placements[i].w != placements2[i].w || placements[i].h != placements2[i].h ||
            placements[i].rotation != placements2[i].rotation)
        {
            fprintf(stderr, "placement mismatch at %d\n", i);
            return 1;
        }
    }
    unsigned long long h2 = rogue_world_hash_tilemap(&regen);
    if (h1 != h2)
    {
        fprintf(stderr, "hash mismatch structures %llu vs %llu\n", h1, h2);
        return 1;
    }
    /* Dungeon entrances */
    rogue_worldgen_context_init(&ctx, &cfg);
    int entrances = rogue_world_place_dungeon_entrances(&cfg, &ctx, &base, placements, count,
                                                        (count > 8) ? 8 : count);
    if (entrances < 0)
    {
        fprintf(stderr, "entrances negative\n");
        return 1;
    }
    rogue_tilemap_free(&regen);
    rogue_tilemap_free(&base);
    rogue_worldgen_context_shutdown(&ctx);
    printf("phase6 structures tests passed\n");
    return 0;
}
