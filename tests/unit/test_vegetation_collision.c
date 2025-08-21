#include "core/app_state.h"
#include "core/vegetation/vegetation.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

/* This test verifies that tree tiles block and plants slow movement. */
int main(void)
{
    if (!rogue_tilemap_init(&g_app.world_map, 64, 64))
    {
        printf("map fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(999u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("gen fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.05f, 4242u);
    int trees = rogue_vegetation_tree_count();
    if (trees == 0)
    {
        printf("no trees\n");
        return 3;
    }
    /* Find a tree tile */
    int tree_tx = -1, tree_ty = -1;
    for (int y = 0; y < g_app.world_map.height && tree_tx < 0; y++)
        for (int x = 0; x < g_app.world_map.width && tree_tx < 0; x++)
        {
            if (rogue_vegetation_tile_blocking(x, y))
            {
                tree_tx = x;
                tree_ty = y;
                break;
            }
        }
    if (tree_tx < 0)
    {
        printf("tree not located\n");
        return 4;
    }
    if (!rogue_vegetation_tile_blocking(tree_tx, tree_ty))
    {
        printf("blocking query fail\n");
        return 5;
    }
    /* Plants slowdown check: we expect some plant tile to have speed scale < 1 */
    int found_slow = 0;
    for (int y = 0; y < g_app.world_map.height && !found_slow; y++)
        for (int x = 0; x < g_app.world_map.width && !found_slow; x++)
        {
            float s = rogue_vegetation_tile_move_scale(x, y);
            if (s < 0.999f)
            {
                found_slow = 1;
            }
        }
    if (!found_slow)
    {
        printf("no slow tiles\n");
        return 6;
    }
    return 0;
}
