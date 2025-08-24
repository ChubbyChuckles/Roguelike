#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include <assert.h>
#include <stdio.h>

/* Contract: SUCCESS when recovery flag true and (optional) within max distance. Resets cooldown
 * timer if provided. */
int main(void)
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);

    /* Keys */
    const char* K_RECOVERY = "target_in_recovery";
    const char* K_AGENT = "agent_pos";
    const char* K_TARGET = "target_pos";
    const char* K_COOLDOWN = "opportunistic_cd";

    rogue_bb_set_bool(&bb, K_RECOVERY, false);
    rogue_bb_set_vec2(&bb, K_AGENT, 0.0f, 0.0f);
    rogue_bb_set_vec2(&bb, K_TARGET, 1.0f, 0.0f);
    rogue_bb_set_timer(&bb, K_COOLDOWN, 3.0f);

    RogueBTNode* leaf = rogue_bt_action_opportunistic_attack(
        "opp_attack", K_RECOVERY, K_AGENT, K_TARGET, /*max_dist*/ 1.5f, K_COOLDOWN);
    assert(leaf);

    RogueBehaviorTree* tree = rogue_behavior_tree_create(leaf);

    /* 1) Not in recovery -> FAILURE, cooldown unchanged */
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);
    float t = 0.0f;
    assert(rogue_bb_get_timer(&bb, K_COOLDOWN, &t));
    assert(t == 3.0f);

    /* 2) In recovery but out of distance (>1.5) -> set target farther, expect FAILURE */
    rogue_bb_set_bool(&bb, K_RECOVERY, true);
    rogue_bb_set_vec2(&bb, K_TARGET, 2.0f, 0.0f); /* distance 2.0 */
    st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);
    assert(rogue_bb_get_timer(&bb, K_COOLDOWN, &t) && t == 3.0f);

    /* 3) In recovery and within distance -> SUCCESS, cooldown reset to 0 */
    rogue_bb_set_vec2(&bb, K_TARGET, 1.0f, 0.0f); /* distance 1.0 */
    st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);
    assert(rogue_bb_get_timer(&bb, K_COOLDOWN, &t) && t == 0.0f);

    rogue_behavior_tree_destroy(tree);
    printf("AI_PHASE6_3_OPPORTUNISTIC_OK\n");
    return 0;
}
