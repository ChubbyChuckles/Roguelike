#include "world_gen_dungeon_theme.h"
#include "world_gen_dungeon_taxonomy.h"
#include <string.h>

/* Simple weighted selection by depth bands; later can use analytics. */
static enum RogueBiomeId pick_biome_for_depth(RogueWorldGenContext* ctx, int depth)
{
    /* Depth bands: 0-2 plains/forest, 3-5 mountain/swamp, 6+ snow/volcanic proxy (mountain). */
    double r = rogue_worldgen_rand_norm(&ctx->biome_rng);
    if (depth < 3)
    {
        return (r < 0.5) ? ROGUE_BIOME_PLAINS : ROGUE_BIOME_FOREST_BIOME;
    }
    else if (depth < 6)
    {
        if (r < 0.4)
            return ROGUE_BIOME_MOUNTAIN_BIOME;
        if (r < 0.8)
            return ROGUE_BIOME_SWAMP_BIOME;
        return ROGUE_BIOME_FOREST_BIOME;
    }
    else
    {
        return (r < 0.6) ? ROGUE_BIOME_SNOW_BIOME : ROGUE_BIOME_MOUNTAIN_BIOME;
    }
}

int rogue_dungeon_select_biome_for_depth(RogueWorldGenContext* ctx, int depth,
                                         enum RogueBiomeId* out_biome)
{
    if (!ctx || !out_biome)
        return 0;
    *out_biome = pick_biome_for_depth(ctx, depth);
    return 1;
}

static RogueDungeonHazardPalette hazard_for_tags(unsigned int tags, int depth)
{
    RogueDungeonHazardPalette h = {0, 0, 0};
    int band = depth / 3;
    if (band > 5)
        band = 5;
    /* Base scaling by depth */
    h.trap_weight = 10 + band * 8;   /* 10..50 */
    h.dot_chance_pct = 5 + band * 6; /* 5..35 */
    h.movement_penalty = (band >= 3) ? 1 : 0;
    /* Tag adjustments */
    if (tags & ROGUE_DUNGEON_THEME_SWAMP)
    {
        h.movement_penalty += 1;
        h.dot_chance_pct += 10; /* miasma */
    }
    if (tags & ROGUE_DUNGEON_THEME_VOLCANIC)
    {
        h.dot_chance_pct += 15;
        h.trap_weight += 10;
    }
    if (tags & ROGUE_DUNGEON_THEME_ICY)
    {
        h.movement_penalty += 1; /* slippery */
    }
    /* Clamp */
    if (h.trap_weight > 100)
        h.trap_weight = 100;
    if (h.dot_chance_pct > 100)
        h.dot_chance_pct = 100;
    if (h.movement_penalty > 3)
        h.movement_penalty = 3;
    return h;
}

void rogue_dungeon_build_theme_profile(RogueWorldGenContext* ctx, int depth,
                                       enum RogueBiomeId biome, RogueDungeonThemeProfile* out)
{
    if (!out)
        return;
    memset(out, 0, sizeof *out);
    out->biome = biome;
    out->theme_tags = rogue_dungeon_biome_theme_tags(biome);
    /* Rare substitution: small chance increases with depth */
    double rare_p = 0.02 + 0.01 * (depth / 3);
    if (rare_p > 0.15)
        rare_p = 0.15;
    out->rare_variant = (rogue_worldgen_rand_norm(&ctx->biome_rng) < rare_p) ? 1 : 0;
    if (out->rare_variant)
    {
        /* Inject a complementary tag based on base biome */
        if (biome == ROGUE_BIOME_PLAINS || biome == ROGUE_BIOME_FOREST_BIOME)
            out->theme_tags |= ROGUE_DUNGEON_THEME_RUIN;
        else if (biome == ROGUE_BIOME_MOUNTAIN_BIOME)
            out->theme_tags |= ROGUE_DUNGEON_THEME_VOLCANIC;
        else if (biome == ROGUE_BIOME_SNOW_BIOME)
            out->theme_tags |= ROGUE_DUNGEON_THEME_ICY; /* reinforce */
        else if (biome == ROGUE_BIOME_SWAMP_BIOME)
            out->theme_tags |= ROGUE_DUNGEON_THEME_DAMP; /* reinforce */
    }

    /* Visual theming: derive fog and ambient color */
    if (out->theme_tags & ROGUE_DUNGEON_THEME_SWAMP)
    {
        out->fog_density = 0.55f;
        out->ambient_r = 60;
        out->ambient_g = 80;
        out->ambient_b = 60;
        out->ambient_sfx_id = 2; /* frogs/crickets */
    }
    else if (out->theme_tags & ROGUE_DUNGEON_THEME_ICY)
    {
        out->fog_density = 0.35f;
        out->ambient_r = 150;
        out->ambient_g = 170;
        out->ambient_b = 190;
        out->ambient_sfx_id = 3; /* wind */
    }
    else if (out->theme_tags & ROGUE_DUNGEON_THEME_VOLCANIC)
    {
        out->fog_density = 0.40f;
        out->ambient_r = 200;
        out->ambient_g = 120;
        out->ambient_b = 80;
        out->ambient_sfx_id = 4; /* lava rumble */
    }
    else if (out->theme_tags & ROGUE_DUNGEON_THEME_FORESTED)
    {
        out->fog_density = 0.30f;
        out->ambient_r = 90;
        out->ambient_g = 120;
        out->ambient_b = 90;
        out->ambient_sfx_id = 5; /* leaves */
    }
    else
    {
        out->fog_density = 0.25f;
        out->ambient_r = 128;
        out->ambient_g = 128;
        out->ambient_b = 128;
        out->ambient_sfx_id = 1; /* default dungeon hum */
    }

    out->hazard = hazard_for_tags(out->theme_tags, depth);
}
