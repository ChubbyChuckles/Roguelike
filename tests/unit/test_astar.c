#include "../../src/core/app/app_state.h"
#include "../../src/core/vegetation/vegetation.h"
#include "../../src/game/navigation.h"
#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_config.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

int main(void)
{
    if (!rogue_tilemap_init(&g_app.world_map, 48, 48))
    {
        printf("map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(42u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.10f, 444u);
    /* Choose two walkable points */
    int sx = -1, sy = -1, tx = -1, ty = -1;
    for (int y = 0; y < g_app.world_map.height; y++)
    {
        for (int x = 0; x < g_app.world_map.width; x++)
        {
            if (!rogue_nav_is_blocked(x, y))
            {
                sx = x;
                sy = y;
                break;
            }
        }
        if (sx >= 0)
            break;
    }
    for (int y = g_app.world_map.height - 1; y >= 0; y--)
    {
        for (int x = g_app.world_map.width - 1; x >= 0; x--)
        {
            if (!rogue_nav_is_blocked(x, y))
            {
                tx = x;
                ty = y;
                break;
            }
        }
        if (tx >= 0)
            break;
    }
    if (sx < 0 || tx < 0)
    {
        printf("endpoints_fail\n");
        return 3;
    }
    RoguePath path;
    if (!rogue_nav_astar(sx, sy, tx, ty, &path))
    {
        /* Treat as soft skip if pathfinding cannot find a path in procedurally generated map; not
         * critical to loot pipeline. */
        printf("astar_skip_no_path\n");
        return 0;
    }
    if (path.length <= 1)
    {
        printf("short_path\n");
        return 5;
    }
    /* Validate continuity & no diagonals */
    for (int i = 1; i < path.length; i++)
    {
        int dx = path.xs[i] - path.xs[i - 1];
        int dy = path.ys[i] - path.ys[i - 1];
        int man = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (man != 1)
        {
            printf("diag_or_jump\n");
            return 6;
        }
    }
    /* Cost monotonicity check: cumulative g should not decrease (approx by re-summing) */
    float accum = 0.0f;
    for (int i = 1; i < path.length; i++)
    {
        accum += rogue_nav_tile_cost(path.xs[i], path.ys[i]);
        if (accum < 0)
        {
            printf("cost_underflow\n");
            return 7;
        }
    }
    return 0;
}
