#include "world/world_gen.h"
#include "world/world_gen_dungeon_theme.h"
#include <stdio.h>
#include <string.h>

static int in_range(int v, int lo, int hi) { return v >= lo && v <= hi; }

int main(void)
{
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 12345u;
    cfg.width = 64;
    cfg.height = 64;
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);

    /* Biome selection determinism: fixed seed -> stable sequence across calls */
    enum RogueBiomeId b0, b1;
    if (!rogue_dungeon_select_biome_for_depth(&ctx, 0, &b0))
    {
        fprintf(stderr, "select biome fail\n");
        return 1;
    }
    /* reseed context to ensure same outcome */
    rogue_worldgen_context_shutdown(&ctx);
    rogue_worldgen_context_init(&ctx, &cfg);
    if (!rogue_dungeon_select_biome_for_depth(&ctx, 0, &b1) || b0 != b1)
    {
        fprintf(stderr, "select biome nondeterministic\n");
        return 1;
    }

    /* Theme profile fields within reasonable ranges and tags coherent with biome */
    RogueDungeonThemeProfile prof;
    rogue_dungeon_build_theme_profile(&ctx, 5, b0, &prof);
    if (prof.biome != b0)
    {
        fprintf(stderr, "biome mismatch\n");
        return 1;
    }
    if (prof.theme_tags == 0)
    {
        fprintf(stderr, "no theme tags\n");
        return 1;
    }
    if (prof.fog_density < 0.0f || prof.fog_density > 1.0f)
    {
        fprintf(stderr, "fog out of range\n");
        return 1;
    }
    if (!in_range(prof.ambient_r, 0, 255) || !in_range(prof.ambient_g, 0, 255) ||
        !in_range(prof.ambient_b, 0, 255))
    {
        fprintf(stderr, "ambient color OOR\n");
        return 1;
    }
    if (!in_range(prof.hazard.trap_weight, 0, 100) ||
        !in_range(prof.hazard.dot_chance_pct, 0, 100) ||
        !in_range(prof.hazard.movement_penalty, 0, 3))
    {
        fprintf(stderr, "hazard OOR\n");
        return 1;
    }

    /* Depth scaling: deeper should generally increase trap_weight and dot chance */
    RogueDungeonThemeProfile shallow, deep;
    rogue_dungeon_build_theme_profile(&ctx, 1, b0, &shallow);
    rogue_dungeon_build_theme_profile(&ctx, 10, b0, &deep);
    if (!(deep.hazard.trap_weight >= shallow.hazard.trap_weight))
    {
        fprintf(stderr, "trap weight not scaling\n");
        return 1;
    }
    if (!(deep.hazard.dot_chance_pct >= shallow.hazard.dot_chance_pct))
    {
        fprintf(stderr, "dot chance not scaling\n");
        return 1;
    }

    rogue_worldgen_context_shutdown(&ctx);
    return 0;
}
