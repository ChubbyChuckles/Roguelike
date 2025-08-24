#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include <assert.h>
#include <stdio.h>

static void test_parry_reaction_window()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    rogue_bb_set_bool(&bb, "incoming", false);
    rogue_bb_set_bool(&bb, "parry_active", false);
    rogue_bb_set_timer(&bb, "parry_t", 0.0f);

    RogueBTNode* parry =
        rogue_bt_action_react_parry("parry", "incoming", "parry_active", "parry_t", 0.12f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(parry);

    /* No threat -> FAILURE and parry inactive */
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);
    bool active = true;
    (void) active;
    assert(rogue_bb_get_bool(&bb, "parry_active", &active) && active == false);

    /* Threat on -> stays SUCCESS within window and sets parry_active */
    rogue_bb_set_bool(&bb, "incoming", true);
    st = rogue_behavior_tree_tick(tree, &bb, 0.08f);
    assert(st == ROGUE_BT_SUCCESS);
    assert(rogue_bb_get_bool(&bb, "parry_active", &active) && active == true);
    st = rogue_behavior_tree_tick(tree, &bb, 0.03f);
    assert(st == ROGUE_BT_SUCCESS);
    /* Exceed window */
    st = rogue_behavior_tree_tick(tree, &bb, 0.05f);
    assert(st == ROGUE_BT_FAILURE);

    rogue_behavior_tree_destroy(tree);
}

static void test_dodge_reaction_duration()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    /* Positions: agent at (5,5), threat at (6,5) => dodge vec should be (-1,0) */
    rogue_bb_set_vec2(&bb, "agent", 5.0f, 5.0f);
    rogue_bb_set_vec2(&bb, "threat", 6.0f, 5.0f);
    rogue_bb_set_bool(&bb, "incoming", false);
    rogue_bb_set_timer(&bb, "dodge_t", 0.0f);

    RogueBTNode* dodge = rogue_bt_action_react_dodge("dodge", "incoming", "agent", "threat", "dvec",
                                                     "dodge_t", 0.10f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(dodge);

    /* No threat -> FAILURE */
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);

    /* Threat on -> SUCCESS for duration, writes vec */
    rogue_bb_set_bool(&bb, "incoming", true);
    st = rogue_behavior_tree_tick(tree, &bb, 0.06f);
    assert(st == ROGUE_BT_SUCCESS);
    RogueBBVec2 v;
    assert(rogue_bb_get_vec2(&bb, "dvec", &v));
    /* direction away from threat */
    assert(v.x < -0.9f && v.x > -1.1f && v.y > -0.1f && v.y < 0.1f);

    /* Still within duration */
    st = rogue_behavior_tree_tick(tree, &bb, 0.03f);
    assert(st == ROGUE_BT_SUCCESS);

    /* Exceed duration */
    st = rogue_behavior_tree_tick(tree, &bb, 0.05f);
    assert(st == ROGUE_BT_FAILURE);

    rogue_behavior_tree_destroy(tree);
}

int main(void)
{
    test_parry_reaction_window();
    test_dodge_reaction_duration();
    printf("test_ai_phase6_2_reactions OK\n");
    return 0;
}
