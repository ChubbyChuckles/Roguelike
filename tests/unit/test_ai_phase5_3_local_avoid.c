#include "../../src/ai/pathing/local_avoidance.h"
#include "../../src/core/app/app_state.h"
#include "../../src/core/navigation.h"
#include "../../src/core/vegetation/vegetation.h"
#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_config.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

static int find_corridor(int* ox, int* oy)
{
    for (int y = 1; y < g_app.world_map.height - 1; y++)
        for (int x = 1; x < g_app.world_map.width - 1; x++)
            if (!rogue_nav_is_blocked(x, y) && rogue_nav_is_blocked(x + 1, y) &&
                rogue_nav_is_blocked(x - 1, y))
            {
                *ox = x;
                *oy = y;
                return 1;
            }
    return 0;
}

int main(void)
{
    if (!rogue_tilemap_init(&g_app.world_map, 48, 48))
    {
        printf("la_map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(321u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("la_gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.12f, 555u);

    int x, y;
    if (!find_corridor(&x, &y))
        return 0;       /* soft skip if not found */
    int dx = 1, dy = 0; /* try to move into wall on the right */
    int rc = rogue_local_avoid_adjust(x, y, &dx, &dy);
    if (rc == -1)
    {
        printf("la_no_move\n");
        return 0;
    }
    if (dx == 1 && dy == 0)
    {
        printf("la_not_adjusted\n");
        return 3;
    }
    /* Ensure chosen detour is unblocked */
    if (rogue_nav_is_blocked(x + dx, y + dy))
    {
        printf("la_adjust_blocked\n");
        return 4;
    }
    return 0;
}
