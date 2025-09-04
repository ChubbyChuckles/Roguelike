#include "world/world_gen.h"
#include "world/world_gen_dungeon_theme.h"
#include <stdio.h>
#include <string.h>

static int run_band(int depth, int trials)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 424242u; /* base seed, varied per trial */
    RogueWorldGenContext ctx;
    int counts[ROGUE_BIOME_MAX] = {0};
    int rare = 0;
    for (int t = 0; t < trials; ++t)
    {
        cfg.seed = 424242u + (unsigned int) t;
        rogue_worldgen_context_init(&ctx, &cfg);
        enum RogueBiomeId b = ROGUE_BIOME_PLAINS;
        if (!rogue_dungeon_select_biome_for_depth(&ctx, depth, &b))
            return 1;
        RogueDungeonThemeProfile prof;
        rogue_dungeon_build_theme_profile(&ctx, depth, b, &prof);
        counts[b]++;
        rare += prof.rare_variant ? 1 : 0;
    }

    /* Basic assertions per band */
    int plains = counts[ROGUE_BIOME_PLAINS];
    int forest = counts[ROGUE_BIOME_FOREST_BIOME];
    int swamp = counts[ROGUE_BIOME_SWAMP_BIOME];
    int mountain = counts[ROGUE_BIOME_MOUNTAIN_BIOME];
    int snow = counts[ROGUE_BIOME_SNOW_BIOME];

    if (depth < 3)
    {
        /* Expect primary mass on plains/forest */
        if (plains + forest < (trials * 60) / 100)
        {
            fprintf(stderr, "shallow band expected >=60%% plains+forest, got %d/%d\n",
                    plains + forest, trials);
            return 2;
        }
    }
    else if (depth < 6)
    {
        /* Expect some mountain/swamp presence */
        if (mountain + swamp < (trials * 30) / 100)
        {
            fprintf(stderr, "mid band expected >=30%% mountain+swamp, got %d/%d\n",
                    mountain + swamp, trials);
            return 3;
        }
    }
    else
    {
        /* Expect snow/mountain dominate */
        if (snow + mountain < (trials * 40) / 100)
        {
            fprintf(stderr, "deep band expected >=40%% snow+mountain, got %d/%d\n", snow + mountain,
                    trials);
            return 4;
        }
    }

    /* Rare variant envelope: ~2% + 1% per 3 depth, clamped 15% */
    double expected = 0.02 + 0.01 * (depth / 3);
    if (expected > 0.15)
        expected = 0.15;
    double rate = (double) rare / (double) trials;
    /* Allow a tolerance band of +/- 50% relative given small trials */
    if (!(rate >= expected * 0.5 && rate <= expected * 1.5))
    {
        fprintf(stderr, "rare rate %.3f outside tolerance vs expected %.3f at depth %d\n", rate,
                expected, depth);
        return 5;
    }
    return 0;
}

int main(void)
{
    int rc = 0;
    rc |= run_band(1, 400);
    rc |= run_band(4, 400);
    rc |= run_band(9, 400);
    return rc;
}
