/* Phase 5 unit tests: river refinement, erosion, bridge hints */
#include "../../src/world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 2025;
    cfg->width = 120;
    cfg->height = 90;
    cfg->noise_octaves = 5;
    cfg->water_level = 0.32;
    cfg->river_sources = 6;
    cfg->river_max_length = 240;
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
    /* Snapshot pre-erosion */
    RogueTileMap before;
    rogue_tilemap_init(&before, cfg.width, cfg.height);
    memcpy(before.tiles, base.tiles, (size_t) cfg.width * cfg.height);
    /* Re-init context for deterministic river refinement independent of macro RNG consumption */
    rogue_worldgen_context_init(&ctx, &cfg);
    if (!rogue_world_refine_rivers(&cfg, &ctx, &base))
    {
        fprintf(stderr, "refine fail\n");
        return 1;
    }
    if (!rogue_world_apply_erosion(&cfg, &ctx, &base, 2, 2))
    {
        fprintf(stderr, "erosion fail\n");
        return 1;
    }
    /* Bridge hints count (min gap 3..6) */
    int hints = rogue_world_mark_bridge_hints(&cfg, &base, 3, 6);
    if (hints <= 0)
    {
        fprintf(stderr, "expected bridge hints >0\n");
        return 1;
    }
    /* Metric: ensure some river deltas or wide tiles exist */
    int river_wide = 0, delta = 0;
    for (int i = 0; i < cfg.width * cfg.height; i++)
    {
        unsigned char t = base.tiles[i];
        if (t == ROGUE_TILE_RIVER_WIDE)
            river_wide++;
        else if (t == ROGUE_TILE_RIVER_DELTA)
            delta++;
    }
    if (river_wide + delta == 0)
    {
        fprintf(stderr, "expected widened or delta rivers\n");
        return 1;
    }
    /* Determinism: regenerate and compare hash */
    unsigned long long h1 = rogue_world_hash_tilemap(&base);
    RogueTileMap regen;
    rogue_tilemap_init(&regen, cfg.width, cfg.height);
    rogue_worldgen_context_init(&ctx, &cfg);
    rogue_world_generate_macro_layout(&cfg, &ctx, &regen, NULL, NULL);
    rogue_worldgen_context_init(&ctx, &cfg);
    rogue_world_refine_rivers(&cfg, &ctx, &regen);
    rogue_world_apply_erosion(&cfg, &ctx, &regen, 2, 2);
    unsigned long long h2 = rogue_world_hash_tilemap(&regen);
    if (h1 != h2)
    {
        fprintf(stderr, "determinism mismatch %llu vs %llu\n", (unsigned long long) h1,
                (unsigned long long) h2);
        return 1;
    }
    rogue_tilemap_free(&before);
    rogue_tilemap_free(&regen);
    rogue_tilemap_free(&base);
    rogue_worldgen_context_shutdown(&ctx);
    printf("phase5 rivers & erosion tests passed\n");
    return 0;
}
