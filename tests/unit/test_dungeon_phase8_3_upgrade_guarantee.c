#include "world_gen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 2468; /* fixed seed for determinism */
    cfg->width = 256;
    cfg->height = 256;
}

static int count_overlay_code(const RogueTileMap* m, unsigned char code)
{
    int c = 0;
    if (!m->overlay_deco || m->overlay_magic != 0xDEC00EAU)
        return 0;
    for (int y = 0; y < m->height; ++y)
        for (int x = 0; x < m->width; ++x)
            if (rogue_tilemap_get_deco(m, x, y) == code)
                c++;
    return c;
}

int main(void)
{
    RogueWorldGenConfig cfg;
    init_cfg(&cfg);
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);
    RogueTileMap map;
    if (!rogue_tilemap_init(&map, cfg.width, cfg.height))
        return 1;

    RogueDungeonGraph graph;
    memset(&graph, 0, sizeof graph);
    assert(rogue_dungeon_generate_graph(&ctx, 24, 20, &graph));

    int ox = (cfg.width - 220) / 2;
    int oy = (cfg.height - 220) / 2;
    if (ox < 0)
        ox = 0;
    if (oy < 0)
        oy = 0;
    rogue_dungeon_carve_into_map(&ctx, &map, &graph, ox, oy, 220, 220);
    rogue_dungeon_place_traps_and_secrets(&ctx, &map, &graph, 8, 0.1);

    /* Case A: force upgrade possible -> expect exactly one upgrade marker when chests placed */
    rogue_dungeon_set_upgrade_possible_override(1);
    RogueDungeonChestPlacement chestsA[64];
    int upA = 0;
    int nA = rogue_dungeon_place_chests(&ctx, &map, &graph, 6, chestsA, 64, &upA);
    assert(nA > 0);
    int markA = count_overlay_code(&map, 14);
    assert(upA == 1);
    assert(markA >= 1);

    /* Record deco count for chest tiers to ensure upgrade marker does not clobber them */
    int tiersA = 0;
    for (int code = 10; code <= 13; ++code)
        tiersA += count_overlay_code(&map, (unsigned char) code);
    assert(tiersA >= nA);

    /* Case B: force upgrade NOT possible -> expect no upgrade marker placed in a fresh map */
    RogueTileMap mapB;
    assert(rogue_tilemap_init(&mapB, cfg.width, cfg.height));
    rogue_dungeon_carve_into_map(&ctx, &mapB, &graph, ox, oy, 220, 220);
    rogue_dungeon_place_traps_and_secrets(&ctx, &mapB, &graph, 8, 0.1);
    rogue_dungeon_set_upgrade_possible_override(0);
    RogueDungeonChestPlacement chestsB[64];
    int upB = 0;
    int nB = rogue_dungeon_place_chests(&ctx, &mapB, &graph, 6, chestsB, 64, &upB);
    assert(nB > 0);
    int markB = count_overlay_code(&mapB, 14);
    assert(upB == 0);
    assert(markB == 0);

    /* Cleanup and clear override */
    rogue_dungeon_set_upgrade_possible_override(-1);
    rogue_dungeon_free_graph(&graph);
    rogue_tilemap_free(&map);
    rogue_tilemap_free(&mapB);
    rogue_worldgen_context_shutdown(&ctx);
    return 0;
}
