/* Phase 12 telemetry & analytics tests */
#include "../../src/world/world_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 9876;
    cfg->width = 80;
    cfg->height = 60;
    cfg->noise_octaves = 3;
    cfg->water_level = 0.30;
}

int main(void)
{
    RogueWorldGenConfig cfg;
    init_cfg(&cfg);
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueTileMap map;
    if (!rogue_tilemap_init(&map, cfg.width, cfg.height))
    {
        fprintf(stderr, "alloc fail\n");
        return 1;
    }
    if (!rogue_world_generate_macro_layout(&cfg, &ctx, &map, NULL, NULL))
    {
        fprintf(stderr, "macro fail\n");
        return 1;
    }
    RogueWorldGenMetrics m;
    if (!rogue_world_metrics_collect(&map, &m))
    {
        fprintf(stderr, "metrics fail\n");
        return 1;
    }
    /* Expect at least some river or anomaly bit1 set if none */
    if (m.rivers == 0 && !(m.anomalies & 0x2))
    {
        fprintf(stderr, "expected anomaly for missing rivers (rivers=%d anomalies=%u)\n", m.rivers,
                m.anomalies);
        return 1;
    }
    unsigned char* heat = (unsigned char*) malloc((size_t) cfg.width * cfg.height);
    if (!heat)
    {
        fprintf(stderr, "alloc heat fail\n");
        return 1;
    }
    if (!rogue_world_export_biome_heatmap(&map, heat, (size_t) cfg.width * cfg.height))
    {
        fprintf(stderr, "heat export fail\n");
        return 1;
    }
    /* Determinism: regenerate and hash heatmap bytes equality */
    RogueWorldGenContext ctx2;
    rogue_worldgen_context_init(&ctx2, &cfg);
    RogueTileMap map2;
    rogue_tilemap_init(&map2, cfg.width, cfg.height);
    if (!rogue_world_generate_macro_layout(&cfg, &ctx2, &map2, NULL, NULL))
    {
        fprintf(stderr, "macro2 fail rivers=%d anomalies=%u\n", m.rivers, m.anomalies);
        return 1;
    }
    unsigned char* heat2 = (unsigned char*) malloc((size_t) cfg.width * cfg.height);
    if (!heat2)
    {
        fprintf(stderr, "alloc heat2 fail\n");
        return 1;
    }
    rogue_world_export_biome_heatmap(&map2, heat2, (size_t) cfg.width * cfg.height);
    if (memcmp(heat, heat2, (size_t) cfg.width * cfg.height) != 0)
    {
        fprintf(stderr, "heatmaps differ\n");
        return 1;
    }
    char buf[64];
    rogue_world_metrics_anomaly_list(&m, buf,
                                     sizeof buf); /* no strict assertion; just exercise path */
    free(heat);
    free(heat2);
    rogue_tilemap_free(&map);
    rogue_tilemap_free(&map2);
    rogue_worldgen_context_shutdown(&ctx);
    rogue_worldgen_context_shutdown(&ctx2);
    printf("phase12 telemetry tests passed\n");
    return 0;
}
