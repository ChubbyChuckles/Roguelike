#include "world_gen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg)
{
    memset(cfg, 0, sizeof *cfg);
    cfg->seed = 1337;
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
    /* depth is derived from the dungeon graph via BFS in the implementation */

    RogueTileMap map;
    if (!rogue_tilemap_init(&map, cfg.width, cfg.height))
    {
        fprintf(stderr, "tilemap init failed\n");
        return 1;
    }

    RogueDungeonGraph graph;
    memset(&graph, 0, sizeof graph);
    if (!rogue_dungeon_generate_graph(&ctx, 24, 20, &graph))
    {
        fprintf(stderr, "graph gen failed\n");
        rogue_tilemap_free(&map);
        return 1;
    }
    /* carve and run Phase 7 traps to ensure rooms exist on tilemap */
    int ox = (cfg.width - 220) / 2;
    int oy = (cfg.height - 220) / 2;
    if (ox < 0)
        ox = 0;
    if (oy < 0)
        oy = 0;
    rogue_dungeon_carve_into_map(&ctx, &map, &graph, ox, oy, 220, 220);
    rogue_dungeon_place_traps_and_secrets(&ctx, &map, &graph, 8, 0.1);

    RogueDungeonChestPlacement chests[64];
    int upgrade = 0;
    int ccount = rogue_dungeon_place_chests(&ctx, &map, &graph, 6, chests, 64, &upgrade);
    if (ccount <= 0)
    {
        fprintf(stderr, "expected chests > 0\n");
        return 1;
    }
    if (upgrade != 1 && upgrade != 0)
    {
        fprintf(stderr, "upgrade flag invalid %d\n", upgrade);
        return 1;
    }
    /* Verify at least one upgrade marker (overlay 14) exists if upgrade==1 */
    int up_markers = count_overlay_code(&map, 14);
    if (upgrade == 1 && up_markers < 1)
    {
        fprintf(stderr, "expected upgrade overlay marker when upgrade==1\n");
        return 1;
    }
    /* Chest tier overlays 10..13 exist and are within expected total */
    int tier_markers = 0;
    for (int code = 10; code <= 13; ++code)
        tier_markers += count_overlay_code(&map, (unsigned char) code);
    if (tier_markers < ccount)
    {
        fprintf(stderr, "tier markers %d < placements %d\n", tier_markers, ccount);
        return 1;
    }

    /* Seed a few material nodes; expect 0..4 but deterministic non-negative */
    int mats = rogue_dungeon_seed_material_nodes(&ctx, &map, &graph, 4);
    if (mats < 0 || mats > 4)
    {
        fprintf(stderr, "materials count out of range %d\n", mats);
        return 1;
    }
    int mat_markers = count_overlay_code(&map, 50);
    if (mat_markers < mats)
    {
        fprintf(stderr, "material markers %d < mats %d\n", mat_markers, mats);
        return 1;
    }

    rogue_dungeon_free_graph(&graph);
    rogue_tilemap_free(&map);
    rogue_worldgen_context_shutdown(&ctx);
    return 0;
}
