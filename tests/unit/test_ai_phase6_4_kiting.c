#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/nodes/advanced_nodes.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

/* Contract: Maintain distance band [min,max]. Move away when too close; toward when too far;
 * SUCCESS when within band. */
int main(void)
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);

    const char* K_AGENT = "agent_pos";
    const char* K_TARGET = "target_pos";

    /* Place target at origin; agent starts at x=0.2 (too close for min=0.5) */
    rogue_bb_set_vec2(&bb, K_TARGET, 0.0f, 0.0f);
    rogue_bb_set_vec2(&bb, K_AGENT, 0.2f, 0.0f);

    RogueBTNode* leaf = rogue_bt_action_kite_band("kite", K_AGENT, K_TARGET, 0.5f, 1.5f, 1.0f);
    assert(leaf);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(leaf);

    /* Step until within band: should move away along +x */
    for (int i = 0; i < 200; ++i)
    {
        RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.1f);
        RogueBBVec2 agent;
        assert(rogue_bb_get_vec2(&bb, K_AGENT, &agent));
        float dx = agent.x - 0.0f;
        float dist = fabsf(dx);
        if (st == ROGUE_BT_SUCCESS)
        {
            assert(dist >= 0.5f - 1e-3f && dist <= 1.5f + 1e-3f);
            break;
        }
        /* While running from 0.2, position should be increasing */
        if (i < 5)
            assert(agent.x > 0.2f);
    }

    /* Now push agent too far and ensure it moves toward target */
    rogue_bb_set_vec2(&bb, K_AGENT, 3.0f, 0.0f);
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.1f);
    assert(st == ROGUE_BT_RUNNING);
    RogueBBVec2 agent;
    assert(rogue_bb_get_vec2(&bb, K_AGENT, &agent));
    assert(agent.x < 3.0f); /* moved toward origin */

    /* If already in band, single tick should return SUCCESS without movement */
    rogue_bb_set_vec2(&bb, K_AGENT, 1.0f, 0.0f);
    assert(rogue_behavior_tree_tick(tree, &bb, 0.1f) == ROGUE_BT_SUCCESS);
    assert(rogue_bb_get_vec2(&bb, K_AGENT, &agent));
    assert(fabsf(agent.x - 1.0f) < 1e-6f);

    rogue_behavior_tree_destroy(tree);
    printf("AI_PHASE6_4_KITING_OK\n");
    return 0;
}
