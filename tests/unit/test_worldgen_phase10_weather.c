/* Phase 10 weather & environmental simulation tests */
#include "world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 24680;
    cfg->width = 64;
    cfg->height = 48;
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
    /* Register weather patterns (rain common, storm rarer, clear baseline) */
    rogue_weather_clear_registry();
    RogueWeatherPatternDesc clear = {"clear", 50, 100, 0.0f, 0.1f, 0xFFFFFFFFu, 5.0f};
    RogueWeatherPatternDesc rain = {"rain", 40, 80, 0.2f, 0.6f, 0xFFFFFFFFu, 10.0f};
    RogueWeatherPatternDesc storm = {"storm", 30, 60, 0.5f, 1.0f, 0xFFFFFFFFu, 2.0f};
    if (rogue_weather_register(&clear) < 0 || rogue_weather_register(&rain) < 0 ||
        rogue_weather_register(&storm) < 0)
    {
        fprintf(stderr, "register fail\n");
        return 1;
    }
    RogueActiveWeather state;
    if (!rogue_weather_init(&ctx, &state))
    {
        fprintf(stderr, "init fail\n");
        return 1;
    }
    int biome_for_test = ROGUE_BIOME_PLAINS; /* pick common biome id */
    int transitions = 0;
    int ticks = 0;
    int observed_counts[3] = {0, 0, 0};
    /* simulate 2000 ticks */
    for (int t = 0; t < 2000; t++)
    {
        int changed = rogue_weather_update(&ctx, &state, 1, biome_for_test);
        if (changed >= 0)
        {
            transitions++;
        }
        if (state.pattern_index >= 0 && state.pattern_index < 3)
        {
            observed_counts[state.pattern_index]++;
        }
        unsigned char r, g, b;
        rogue_weather_sample_lighting(&state, &r, &g, &b);
        float mv = rogue_weather_movement_factor(&state);
        if (mv < 0.5f || mv > 1.0f)
        {
            fprintf(stderr, "movement factor out of range %f\n", mv);
            return 1;
        }
        ticks++;
    }
    if (transitions == 0)
    {
        fprintf(stderr, "expected some transitions (obs clear=%d rain=%d storm=%d)\n",
                observed_counts[0], observed_counts[1], observed_counts[2]);
        return 1;
    }
    int total_obs = observed_counts[0] + observed_counts[1] + observed_counts[2];
    if (total_obs == 0)
    {
        fprintf(stderr, "no observations (obs clear=%d rain=%d storm=%d)\n", observed_counts[0],
                observed_counts[1], observed_counts[2]);
        return 1;
    }
    /* Probability weighting rough check: rain (weight 10) should on average meet or exceed clear
     * (weight 5). Allow tolerance (>=80%). */
    if (observed_counts[1] * 100 < observed_counts[0] * 80)
    {
        fprintf(stderr,
                "rain count unexpectedly low vs clear (%d vs %d; storm=%d transitions=%d)\n",
                observed_counts[1], observed_counts[0], observed_counts[2], transitions);
        return 1;
    }
    /* Determinism test: reinitialize and replay tick loop -> identical observed_counts distribution
     */
    RogueWorldGenContext ctx2;
    rogue_worldgen_context_init(&ctx2, &cfg);
    RogueActiveWeather state2;
    rogue_weather_init(&ctx2, &state2);
    /* replicate macro generation RNG consumption to keep channel parity */
    RogueTileMap map2;
    if (!rogue_tilemap_init(&map2, cfg.width, cfg.height))
    {
        fprintf(stderr, "alloc2 fail\n");
        return 1;
    }
    if (!rogue_world_generate_macro_layout(&cfg, &ctx2, &map2, NULL, NULL))
    {
        fprintf(stderr, "macro2 fail\n");
        return 1;
    }
    rogue_weather_clear_registry(); /* identical registry again */
    rogue_weather_register(&clear);
    rogue_weather_register(&rain);
    rogue_weather_register(&storm);
    int observed_counts2[3] = {0, 0, 0};
    for (int t = 0; t < 2000; t++)
    {
        rogue_weather_update(&ctx2, &state2, 1, biome_for_test);
        if (state2.pattern_index >= 0 && state2.pattern_index < 3)
        {
            observed_counts2[state2.pattern_index]++;
        }
    }
    for (int i = 0; i < 3; i++)
    {
        if (observed_counts[i] != observed_counts2[i])
        {
            fprintf(stderr,
                    "nondeterministic distribution index %d %d vs %d (orig counts: clear=%d "
                    "rain=%d storm=%d)\n",
                    i, observed_counts[i], observed_counts2[i], observed_counts[0],
                    observed_counts[1], observed_counts[2]);
            return 1;
        }
    }
    rogue_tilemap_free(&map);
    rogue_worldgen_context_shutdown(&ctx);
    rogue_tilemap_free(&map2);
    rogue_worldgen_context_shutdown(&ctx2);
    printf("phase10 weather tests passed\n");
    return 0;
}
