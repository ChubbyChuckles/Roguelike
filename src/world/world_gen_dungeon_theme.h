#ifndef ROGUE_WORLD_GEN_DUNGEON_THEME_H
#define ROGUE_WORLD_GEN_DUNGEON_THEME_H

#include "world_gen.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 3: Biome & Theme Layering */

    typedef struct RogueDungeonHazardPalette
    {
        int movement_penalty; /* 0..3, qualitative movement penalty level */
        int dot_chance_pct;   /* 0..100, chance environmental DoT present */
        int trap_weight;      /* 0..100, relative trap density target */
    } RogueDungeonHazardPalette;

    typedef struct RogueDungeonThemeProfile
    {
        enum RogueBiomeId biome; /* selected biome basis */
        unsigned int theme_tags; /* taxonomy-derived tags (see world_gen_dungeon_taxonomy.h) */
        float fog_density;       /* 0..1 */
        unsigned char ambient_r; /* ambient light color hint */
        unsigned char ambient_g;
        unsigned char ambient_b;
        int ambient_sfx_id; /* small integer id for ambient loop selection */
        RogueDungeonHazardPalette hazard;
        int rare_variant; /* 1 if rare biome substitution applied */
    } RogueDungeonThemeProfile;

    /* Select a biome for a dungeon floor/segment based on depth. Returns 1 on success. */
    int rogue_dungeon_select_biome_for_depth(RogueWorldGenContext* ctx, int depth,
                                             enum RogueBiomeId* out_biome);

    /* Build a theme profile for the given biome/depth. Deterministic for ctx->seed. */
    void rogue_dungeon_build_theme_profile(RogueWorldGenContext* ctx, int depth,
                                           enum RogueBiomeId biome, RogueDungeonThemeProfile* out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_WORLD_GEN_DUNGEON_THEME_H */
