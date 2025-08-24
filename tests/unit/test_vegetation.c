#include "../../src/core/app_state.h"
#include "../../src/core/vegetation/vegetation.h"
#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_config.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

static int bootstrap_world(void)
{
    if (!rogue_tilemap_init(&g_app.world_map, 96, 96))
        return 0;
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(777u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
        return 0;
    return 1;
}

int main(void)
{
    if (!bootstrap_world())
    {
        printf("world fail\n");
        return 1;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.10f, 777u);
    int total = rogue_vegetation_count();
    if (total <= 0)
    {
        printf("no vegetation\n");
        return 2;
    }
    int trees = rogue_vegetation_tree_count();
    int plants = rogue_vegetation_plant_count();
    if (trees <= 0 || plants <= 0)
    {
        printf("expected both trees and plants present\n");
        return 3;
    }
    float cover_before = rogue_vegetation_get_tree_cover();
    rogue_vegetation_set_tree_cover(cover_before + 0.05f);
    if (rogue_vegetation_get_tree_cover() <= cover_before)
    {
        printf("cover did not increase\n");
        return 4;
    }
    rogue_vegetation_set_tree_cover(0.0f);
    if (rogue_vegetation_get_tree_cover() != 0.0f)
    {
        printf("cover not zeroed\n");
        return 5;
    }
    rogue_vegetation_shutdown();
    return 0;
}
