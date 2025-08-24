#include "../../src/core/app_state.h"
#include "../../src/core/navigation.h"
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
        printf("map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(555u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.12f, 888u);
    int found_cost_variant = 0;
    for (int y = 0; y < g_app.world_map.height && !found_cost_variant; y++)
        for (int x = 0; x < g_app.world_map.width && !found_cost_variant; x++)
        {
            float c = rogue_nav_tile_cost(x, y);
            if (c > 1.01f && c < 5.0f)
                found_cost_variant = 1;
        }
    if (!found_cost_variant)
    {
        printf("no_cost_variant\n");
        return 3;
    }
    // Step test: ensure step never diagonal
    int dx, dy;
    rogue_nav_cardinal_step_towards(5.0f, 5.0f, 20.0f, 20.0f, &dx, &dy);
    if (dx != 0 && dy != 0)
    {
        printf("diag_step\n");
        return 4;
    }
    return 0;
}
