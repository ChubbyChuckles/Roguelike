/* Prevent SDL from redefining main to SDL_main (we link without SDL2main here) */
#define SDL_MAIN_HANDLED 1
#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/core/app/app_state.h"
#include "../../src/entities/enemy.h"
#include <assert.h>
#include <stdlib.h>

/* Minimal harness: create one enemy, enable BT, simulate ticks moving toward (0,0) player */
static int test_run(void)
{
    /* Initialize dummy player and enemy */
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;
    RogueEnemy e = {0};
    e.base.pos.x = 5.0f;
    e.base.pos.y = 0.0f;
    e.alive = 1;
    e.ai_state = ROGUE_ENEMY_AI_AGGRO;
    rogue_enemy_ai_bt_enable(&e);
    assert(e.ai_bt_enabled && e.ai_tree);
    float start = e.base.pos.x;
    for (int i = 0; i < 120; i++)
    { /* simulate 2 seconds at ~16ms */
        rogue_enemy_ai_bt_tick(&e, 0.016f);
    }
    /* Expect enemy moved closer to player */
    assert(e.base.pos.x < start);
    rogue_enemy_ai_bt_disable(&e);
    return 0;
}

int main() { return test_run(); }
