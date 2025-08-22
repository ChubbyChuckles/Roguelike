#include "core/app_state.h"
#include "core/navigation.h"
#include "core/vegetation/vegetation.h"
#include "world/world_gen.h"
#include "world/world_gen_config.h"
#include <stdio.h>
#include <string.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }
void rogue_skill_tree_register_baseline(void) {}

static int first_walkable(int* outx, int* outy)
{
    for (int y = 0; y < g_app.world_map.height; ++y)
        for (int x = 0; x < g_app.world_map.width; ++x)
            if (!rogue_nav_is_blocked(x, y))
            {
                *outx = x;
                *outy = y;
                return 1;
            }
    return 0;
}

static int last_walkable(int* outx, int* outy)
{
    for (int y = g_app.world_map.height - 1; y >= 0; --y)
        for (int x = g_app.world_map.width - 1; x >= 0; --x)
            if (!rogue_nav_is_blocked(x, y))
            {
                *outx = x;
                *outy = y;
                return 1;
            }
    return 0;
}

int main(void)
{
    if (!rogue_tilemap_init(&g_app.world_map, 48, 48))
    {
        printf("map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(24601u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.11f, 13579u);

    int sx = -1, sy = -1, tx = -1, ty = -1;
    if (!first_walkable(&sx, &sy) || !last_walkable(&tx, &ty))
    {
        printf("endpoints_fail\n");
        return 3;
    }

    RoguePath raw;
    if (!rogue_nav_astar(sx, sy, tx, ty, &raw))
    {
        printf("astar_skip_no_path\n");
        return 0; /* soft skip like test_astar */
    }
    if (raw.length < 2)
    {
        printf("path_too_short\n");
        return 4;
    }

    RoguePath simple;
    int slen = rogue_nav_path_simplify(&raw, &simple);
    if (slen <= 0)
    {
        printf("simplify_fail\n");
        return 5;
    }
    if (slen > raw.length)
    {
        printf("simplify_longer\n");
        return 6;
    }
    /* Simplified path must preserve endpoints */
    if (simple.xs[0] != raw.xs[0] || simple.ys[0] != raw.ys[0] ||
        simple.xs[simple.length - 1] != raw.xs[raw.length - 1] ||
        simple.ys[simple.length - 1] != raw.ys[raw.length - 1])
    {
        printf("endpoints_changed\n");
        return 7;
    }
    /* Simplified path steps must still be cardinal */
    for (int i = 1; i < simple.length; ++i)
    {
        int dx = simple.xs[i] - simple.xs[i - 1];
        int dy = simple.ys[i] - simple.ys[i - 1];
        int man = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (man != 1)
        {
            printf("non_cardinal\n");
            return 8;
        }
    }

    printf("ok\n");
    return 0;
}
