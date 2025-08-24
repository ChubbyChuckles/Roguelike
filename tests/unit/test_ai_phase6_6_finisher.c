#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include <assert.h>

static void test_finisher_threshold_and_distance()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    const char* kTHP = "t_hp";
    const char* kA = "ap";
    const char* kT = "tp";
    const char* kCD = "cool";
    rogue_bb_set_float(&bb, kTHP, 50.0f);
    rogue_bb_set_vec2(&bb, kA, 0.0f, 0.0f);
    rogue_bb_set_vec2(&bb, kT, 10.0f, 0.0f);
    rogue_bb_set_timer(&bb, kCD, 123.0f);

    RogueBTNode* fin = rogue_bt_action_finisher_execute("fin", kTHP, 25.0f, kA, kT, 6.0f, kCD);
    RogueBehaviorTree* bt = rogue_behavior_tree_create(fin);

    /* Above threshold -> fail */
    RogueBTStatus st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);

    /* Below threshold but out of range -> fail */
    rogue_bb_set_float(&bb, kTHP, 10.0f);
    st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);

    /* Move into range -> success and cooldown reset */
    rogue_bb_set_vec2(&bb, kT, 3.0f, 4.0f); /* distance = 5 */
    st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);
    float c = 1.0f;
    rogue_bb_get_timer(&bb, kCD, &c);
    assert(c == 0.0f);

    rogue_behavior_tree_destroy(bt);
}

int main()
{
    test_finisher_threshold_and_distance();
    return 0;
}
