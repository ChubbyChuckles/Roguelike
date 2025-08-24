#include "../../src/ai/pathing/flow_field.h"
#include "../../src/core/app/app_state.h"
#include "../../src/core/vegetation/vegetation.h"
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
        printf("ff_map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(123u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("ff_gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.10f, 777u);

    /* Find target and a few random reachable samples */
    int tx = -1, ty = -1;
    for (int y = 0; y < g_app.world_map.height && ty < 0; y++)
        for (int x = 0; x < g_app.world_map.width; x++)
            if (!rogue_nav_is_blocked(x, y))
            {
                tx = x;
                ty = y;
                break;
            }
    if (tx < 0)
    {
        printf("ff_no_target\n");
        return 0;
    }

    RogueFlowField ff;
    if (!rogue_flow_field_build(tx, ty, &ff))
    {
        printf("ff_build_fail\n");
        return 3;
    }

    /* Dist at target is 0, finite around reachable cells */
    if (!(ff.dist[ty * ff.width + tx] == 0.0f))
    {
        printf("ff_target_dist_nonzero\n");
        return 4;
    }

    /* Walk from a far reachable point toward target using steps; distance should decrease */
    int sx = -1, sy = -1;
    for (int y = g_app.world_map.height - 1; y >= 0 && sy < 0; y--)
        for (int x = g_app.world_map.width - 1; x >= 0; x--)
            if (!rogue_nav_is_blocked(x, y) && ff.dist[y * ff.width + x] < 1e20f)
            {
                sx = x;
                sy = y;
                break;
            }
    if (sx < 0)
    {
        rogue_flow_field_free(&ff);
        return 0;
    }
    float prev = ff.dist[sy * ff.width + sx];
    int x = sx, y = sy, it = 0;
    while ((x != tx || y != ty) && it++ < 500)
    {
        int dx = 0, dy = 0;
        int rc = rogue_flow_field_step(&ff, x, y, &dx, &dy);
        if (rc != 0)
            break;
        x += dx;
        y += dy;
        float cur = ff.dist[y * ff.width + x];
        if (cur > prev + 1e-3f)
        {
            printf("ff_non_monotonic\n");
            rogue_flow_field_free(&ff);
            return 5;
        }
        prev = cur;
    }
    rogue_flow_field_free(&ff);
    return 0;
}
