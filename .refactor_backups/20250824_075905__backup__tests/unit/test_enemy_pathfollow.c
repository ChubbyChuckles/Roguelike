#include "../../src/core/app/app_state.h"
#include "../../src/core/enemy/enemy_system.h"
#include "../../src/core/navigation.h"
#include "../../src/core/vegetation/vegetation.h"
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/world/world_gen.h"
#include "../../src/world/world_gen_config.h"
#include <stdio.h>
#include <string.h>

/* Use core definitions from rogue_core library instead of redefining globals / functions here to
 * avoid duplicate symbol linker errors. */

/* This test ensures an aggro enemy far from the player obtains a multi-step A* path and advances
 * along it (cardinal non-diagonal moves). */
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("start\n");
    if (!rogue_tilemap_init(&g_app.world_map, 48, 48))
    {
        printf("map_fail\n");
        return 1;
    }
    RogueWorldGenConfig cfg = rogue_world_gen_config_build(999u, 0, 0);
    if (!rogue_world_generate(&g_app.world_map, &cfg))
    {
        printf("gen_fail\n");
        return 2;
    }
    rogue_vegetation_init();
    rogue_vegetation_load_defs("../assets/plants.cfg", "../assets/trees.cfg");
    rogue_vegetation_generate(0.10f, 123u);
    printf("after_gen\n");
    /* Create enemy type */
    g_app.enemy_type_count = 1;
    g_app.per_type_counts[0] = 0;
    RogueEnemyTypeDef* t = &g_app.enemy_types[0];
    memset(t, 0, sizeof *t);
    t->speed = 4.0f;
    t->patrol_radius = 3;
    t->aggro_radius = 120;
    t->group_min = 1;
    t->group_max = 1;
    t->pop_target = 0; /* large radius to keep distant enemy aggro */
    g_app.dt = 0.016f; /* ~60 FPS */
    /* Place player at near origin (choose walkable tile) */
    int px = -1, py = -1;
    for (int y = 0; y < g_app.world_map.height && px < 0; y++)
        for (int x = 0; x < g_app.world_map.width && px < 0; x++)
        {
            if (!rogue_nav_is_blocked(x, y))
            {
                px = x;
                py = y;
            }
        }
    if (px < 0)
    {
        printf("no_player_spot\n");
        return 3;
    }
    g_app.player.base.pos.x = (float) px;
    g_app.player.base.pos.y = (float) py;
    g_app.player.health = 10;
    g_app.player.max_health = 10;
    printf("player at %d,%d\n", px, py);
    /* Place enemy at distant walkable tile */
    int ex = -1, ey = -1;
    for (int y = g_app.world_map.height - 1; y >= 0 && ex < 0; y--)
        for (int x = g_app.world_map.width - 1; x >= 0 && ex < 0; x--)
        {
            if (!rogue_nav_is_blocked(x, y))
            {
                ex = x;
                ey = y;
            }
        }
    if (ex < 0)
    {
        printf("no_enemy_spot\n");
        return 4;
    }
    RogueEnemy* e = &g_app.enemies[0];
    memset(e, 0, sizeof *e);
    e->alive = 1;
    e->type_index = 0;
    e->base.pos.x = (float) ex;
    e->base.pos.y = (float) ey;
    e->anchor_x = e->base.pos.x;
    e->anchor_y = e->base.pos.y;
    e->patrol_target_x = e->base.pos.x;
    e->patrol_target_y = e->base.pos.y;
    e->ai_state = ROGUE_ENEMY_AI_AGGRO;
    e->max_health = 5;
    e->health = 5;
    g_app.enemy_count = 1;
    g_app.per_type_counts[0] = 1;
    printf("enemy at %d,%d\n", ex, ey);
    /* Disable spawner */ g_app.enemy_type_count = 0;
    int observed_advance = 0;
    int last_pos_tx = (int) (e->base.pos.x + 0.5f);
    int last_pos_ty = (int) (e->base.pos.y + 0.5f);
    for (int frame = 0; frame < 240; ++frame)
    { /* up to ~4 seconds */
        if (frame == 0)
            printf("loop_begin\n");
        rogue_enemy_system_update(16.0f);
        int ptx = (int) (g_app.player.base.pos.x + 0.5f);
        int pty = (int) (g_app.player.base.pos.y + 0.5f);
        if (frame < 4)
            printf("f%d player=(%d,%d) enemy_tile=(%d,%d)\n", frame, ptx, pty,
                   (int) (e->base.pos.x + 0.5f), (int) (e->base.pos.y + 0.5f));
        int cur_tx = (int) (e->base.pos.x + 0.5f);
        int cur_ty = (int) (e->base.pos.y + 0.5f);
        if ((cur_tx != last_pos_tx || cur_ty != last_pos_ty))
        {
            /* ensure cardinal step */
            int dx = cur_tx - last_pos_tx;
            int dy = cur_ty - last_pos_ty;
            int man = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (man != 1)
            {
                printf("non_cardinal_step\n");
                return 5;
            }
            observed_advance = 1;
            last_pos_tx = cur_tx;
            last_pos_ty = cur_ty;
        }
        if (cur_tx == px && cur_ty == py)
            break; /* reached player */
    }
    if (!observed_advance)
    {
        printf("no_movement ex=%f ey=%f p=(%d,%d)\n", e->base.pos.x, e->base.pos.y, px, py);
        return 7;
    }
    printf("ok enemy advanced toward player tile=(%d,%d) player=(%d,%d)\n",
           (int) (e->base.pos.x + 0.5f), (int) (e->base.pos.y + 0.5f), px, py);
    return 0;
}
