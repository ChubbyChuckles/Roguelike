#include "../../src/core/app/app_state.h"
#include "../../src/core/vegetation/vegetation.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

/* Create a single tree def manually, then an instance, test radius blocking. */
int main(void)
{
    rogue_vegetation_init();
    /* Use generation path to populate defs (cannot access internal static arrays). */
    /* Fallback: load real defs and pick a tree to test radius logic */
    if (!rogue_tilemap_init(&g_app.world_map, 32, 32))
    {
        printf("map fail\n");
        return 1;
    }
    for (int i = 0; i < 32 * 32; i++)
        g_app.world_map.tiles[i] = ROGUE_TILE_GRASS;
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.08f, 1234u);
    int cx, cy, rad;
    if (!rogue_vegetation_first_tree(&cx, &cy, &rad))
    {
        printf("no tree\n");
        return 0;
    }
    if (!rogue_vegetation_tile_blocking(cx, cy))
    {
        printf("center not blocking\n");
        return 2;
    }
    /* Accept blocking within radius; ensure outside radius+2 mostly free */
    int outside_hits = 0, samples = 0;
    int limit = rad + 2;
    for (int dy = -limit - 2; dy <= limit + 2; dy++)
        for (int dx = -limit - 2; dx <= limit + 2; dx++)
        {
            if (dx * dx + dy * dy <= (limit * limit))
                continue; /* only check outside shell */
            int qx = cx + dx, qy = cy + dy;
            if (qx < 0 || qy < 0 || qx >= g_app.world_map.width || qy >= g_app.world_map.height)
                continue;
            samples++;
            if (rogue_vegetation_tile_blocking(qx, qy))
                outside_hits++;
        }
    if (samples > 0 && outside_hits > samples / 4)
    {
        printf("too many outside blocks (%d/%d)\n", outside_hits, samples);
        return 3;
    }
    return 0;
}
