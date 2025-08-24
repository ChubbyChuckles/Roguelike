#define SDL_MAIN_HANDLED 1
#include "../../src/ai/core/ai_agent_pool.h"
#include "../../src/ai/core/ai_scheduler.h"
#include "../../src/core/app/app_state.h"
#include "../../src/entities/enemy.h"
#include <stdio.h>
#include <string.h>

extern void rogue_enemy_ai_bt_enable(RogueEnemy* e);

int main(void)
{
    /* Reset systems */
    rogue_ai_scheduler_reset_for_tests();
    rogue_ai_agent_pool_reset_for_tests();
    rogue_ai_scheduler_set_buckets(8); /* distribute load */
    rogue_ai_lod_set_radius(30.0f);    /* initial LOD radius */
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;

#define AI_STRESS_ENEMY_COUNT 200
    const int ENEMY_COUNT = AI_STRESS_ENEMY_COUNT; /* stress target */
    RogueEnemy enemies[AI_STRESS_ENEMY_COUNT];
    float start_x[AI_STRESS_ENEMY_COUNT];
    for (int i = 0; i < ENEMY_COUNT; i++)
    {
        memset(&enemies[i], 0, sizeof enemies[i]);
        enemies[i].alive = 1;
        enemies[i].base.pos.x = (float) (i + 1); /* 1..200 */
        enemies[i].base.pos.y = 0.0f;
        rogue_enemy_ai_bt_enable(&enemies[i]);
        start_x[i] = enemies[i].base.pos.x;
    }
    /* Phase 1: simulate some frames with limited LOD; near (<=30) should move, far should not */
    for (int f = 0; f < 64; ++f)
    {
        rogue_ai_scheduler_tick(enemies, ENEMY_COUNT, 0.05f);
    }
    int moved_near = 0, static_far = 0;
    for (int i = 0; i < ENEMY_COUNT; i++)
    {
        if (start_x[i] <= 30.0f)
        {
            if (enemies[i].base.pos.x < start_x[i] - 0.01f)
                moved_near++;
        }
        else
        {
            if (enemies[i].base.pos.x >= start_x[i] - 0.01f)
                static_far++;
        }
    }
    if (moved_near < 25)
    {
        printf("AI_STRESS_FAIL near_not_moving moved=%d\n", moved_near);
        return 1;
    }
    if (static_far < 160 - 30)
    { /* expect majority of 170 far enemies untouched */
        printf("AI_STRESS_FAIL far_moved_early static_far=%d\n", static_far);
        return 1;
    }

    /* Phase 2: expand LOD radius so far enemies start moving */
    rogue_ai_lod_set_radius(400.0f);
    for (int f = 0; f < 64; ++f)
    {
        rogue_ai_scheduler_tick(enemies, ENEMY_COUNT, 0.05f);
    }
    int moved_far = 0;
    for (int i = 0; i < ENEMY_COUNT; i++)
        if (start_x[i] > 30.0f)
        {
            if (enemies[i].base.pos.x < start_x[i] - 0.01f)
                moved_far++;
        }
    if (moved_far < 100)
    {
        printf("AI_STRESS_FAIL far_not_moving moved_far=%d\n", moved_far);
        return 1;
    }

    int peak = rogue_ai_agent_pool_peak();
    if (peak < ENEMY_COUNT)
    {
        printf("AI_STRESS_FAIL pool_peak %d\n", peak);
        return 1;
    }

    printf("AI_STRESS_OK enemies=%d buckets=%d moved_near=%d moved_far=%d peak_pool=%d frame=%u\n",
           ENEMY_COUNT, rogue_ai_scheduler_get_buckets(), moved_near, moved_far, peak,
           rogue_ai_scheduler_frame());
    return 0;
}
