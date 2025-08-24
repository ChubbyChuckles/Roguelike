#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include <assert.h>

/* A trivial child that always succeeds */
static RogueBTStatus tick_trivial(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) node;
    (void) bb;
    (void) dt;
    return ROGUE_BT_SUCCESS;
}
static RogueBTNode* make_trivial(const char* name)
{
    return rogue_bt_node_create(name, 0, tick_trivial);
}

static void test_reaction_delay()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    const char* kRT = "rt";
    rogue_bb_set_timer(&bb, kRT, 0.0f);
    RogueBTNode* child = make_trivial("child");
    RogueBTNode* deco = rogue_bt_decorator_reaction_delay("delay", child, kRT, 0.05f);
    RogueBehaviorTree* bt = rogue_behavior_tree_create(deco);

    RogueBTStatus st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);
    /* Tick until reaction timer elapses; should succeed within a few frames */
    for (int i = 0; i < 8 && st != ROGUE_BT_SUCCESS; ++i)
    {
        st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    }
    /* Timer should have reached threshold */
    float tt = 0.0f;
    assert(rogue_bb_get_timer(&bb, kRT, &tt));
    assert(tt >= 0.05f);
    assert(st == ROGUE_BT_SUCCESS);

    rogue_behavior_tree_destroy(bt);
}

static void test_aggression_gate()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    const char* kAgg = "agg";
    rogue_bb_set_float(&bb, kAgg, 0.0f);
    RogueBTNode* child = make_trivial("child");
    RogueBTNode* deco = rogue_bt_decorator_aggression_gate("ag", child, kAgg, 0.5f);
    RogueBehaviorTree* bt = rogue_behavior_tree_create(deco);

    RogueBTStatus st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);
    rogue_bb_set_float(&bb, kAgg, 0.6f);
    st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);

    rogue_behavior_tree_destroy(bt);
}

int main()
{
    test_reaction_delay();
    test_aggression_gate();
    return 0;
}
