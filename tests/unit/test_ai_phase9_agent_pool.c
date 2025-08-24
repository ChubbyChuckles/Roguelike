#define SDL_MAIN_HANDLED 1
#include "../../src/ai/core/ai_agent_pool.h"
#include "../../src/core/app_state.h"
#include "../../src/entities/enemy.h"
#include <stdio.h>
#include <string.h>

/* Minimal stub enabling/disabling BT to exercise pool: use existing enable API. */
extern void rogue_enemy_ai_bt_enable(RogueEnemy* e);
extern void rogue_enemy_ai_bt_disable(RogueEnemy* e);

static void init_enemy(RogueEnemy* e)
{
    memset(e, 0, sizeof *e);
    e->alive = 1;
}

int main(void)
{
    printf("AI_POOL_DBG start slab=%zu\n", rogue_ai_agent_pool_slab_size());
    fflush(stdout);
    rogue_ai_agent_pool_reset_for_tests();
    int start_peak = rogue_ai_agent_pool_peak();
    if (start_peak != 0)
    {
        printf("AI_POOL_FAIL nonzero_start_peak %d\n", start_peak);
        return 1;
    }
    /* Acquire & release several enemies repeatedly to force reuse */
    RogueEnemy enemies[16];
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        for (int i = 0; i < 16; i++)
        {
            init_enemy(&enemies[i]);
            rogue_enemy_ai_bt_enable(&enemies[i]);
        }
        printf("AI_POOL_DBG cycle=%d after_enable in_use=%d\n", cycle,
               rogue_ai_agent_pool_in_use());
        fflush(stdout);
        int in_use = rogue_ai_agent_pool_in_use();
        if (in_use != 16)
        {
            printf("AI_POOL_FAIL unexpected_in_use %d\n", in_use);
            return 1;
        }
        for (int i = 0; i < 16; i++)
        {
            rogue_enemy_ai_bt_disable(&enemies[i]);
        }
        printf("AI_POOL_DBG cycle=%d after_disable in_use=%d free=%d\n", cycle,
               rogue_ai_agent_pool_in_use(), rogue_ai_agent_pool_free());
        fflush(stdout);
        if (rogue_ai_agent_pool_in_use() != 0)
        {
            printf("AI_POOL_FAIL leak_in_use %d\n", rogue_ai_agent_pool_in_use());
            return 1;
        }
    }
    int free_ct = rogue_ai_agent_pool_free();
    int peak = rogue_ai_agent_pool_peak();
    if (peak != 16)
    {
        printf("AI_POOL_FAIL peak_mismatch %d\n", peak);
        return 1;
    }
    if (free_ct < 16)
    {
        printf("AI_POOL_FAIL free_ct %d\n", free_ct);
        return 1;
    }
    /* Re-acquire a subset and ensure we don't grow peak */
    for (int i = 0; i < 4; i++)
    {
        init_enemy(&enemies[i]);
        rogue_enemy_ai_bt_enable(&enemies[i]);
    }
    if (rogue_ai_agent_pool_peak() != peak)
    {
        printf("AI_POOL_FAIL peak_grew %d->%d\n", peak, rogue_ai_agent_pool_peak());
        return 1;
    }
    printf("AI_POOL_OK peak=%d free_after=%d reuse_ok=1 in_use=%d\n", peak, free_ct,
           rogue_ai_agent_pool_in_use());
    return 0;
}
