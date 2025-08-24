#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include <assert.h>
#include <stdlib.h>

typedef struct
{
    const char* pos_key;
    int* tick_counter;
} CountLeafCfg;

static RogueBTStatus tick_count_succ(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
    CountLeafCfg* cfg = (CountLeafCfg*) n->user_data;
    (*cfg->tick_counter)++;
    return ROGUE_BT_SUCCESS;
}

static RogueBTNode* make_counting_success(const char* name, const char* pos_key, int* counter)
{
    RogueBTNode* leaf = rogue_bt_node_create(name, 0, tick_count_succ);
    CountLeafCfg* cfg = (CountLeafCfg*) calloc(1, sizeof(CountLeafCfg));
    cfg->pos_key = pos_key;
    cfg->tick_counter = counter;
    leaf->user_data = cfg;
    return leaf;
}

static void test_stuck_stationary_then_move()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    const char* POS = "agent_pos";
    const char* TIMER = "stuck_timer";
    /* Seed initial position */
    rogue_bb_set_vec2(&bb, POS, 10.0f, 5.0f);
    rogue_bb_set_timer(&bb, TIMER, 0.0f);

    int child_ticks = 0;
    RogueBTNode* child = make_counting_success("child", POS, &child_ticks);
    /* Window 0.10s, threshold 0.05 units; dt 0.04s => fail on 3rd stationary tick */
    RogueBTNode* stuck = rogue_bt_decorator_stuck_detect("stuck", child, POS, TIMER, 0.10f, 0.05f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(stuck);

    RogueBTStatus st;
    st = rogue_behavior_tree_tick(tree, &bb, 0.04f);
    assert(st == ROGUE_BT_SUCCESS && child_ticks == 1);
    st = rogue_behavior_tree_tick(tree, &bb, 0.04f);
    assert(st == ROGUE_BT_SUCCESS && child_ticks == 2);
    st = rogue_behavior_tree_tick(tree, &bb, 0.04f);
    assert(st == ROGUE_BT_FAILURE && child_ticks == 2); /* failure without ticking child */

    /* Move agent enough to reset window; next tick should pass through to child */
    rogue_bb_set_vec2(&bb, POS, 11.0f, 5.0f); /* dx = 1.0 > threshold */
    st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS && child_ticks == 3);

    /* Stationary again; accumulate until next failure */
    st = rogue_behavior_tree_tick(tree, &bb, 0.05f);
    assert(st == ROGUE_BT_SUCCESS && child_ticks == 4);
    st = rogue_behavior_tree_tick(tree, &bb, 0.05f);
    assert(st == ROGUE_BT_FAILURE && child_ticks == 4);

    rogue_behavior_tree_destroy(tree);
}

int main(void)
{
    test_stuck_stationary_then_move();
    return 0;
}
