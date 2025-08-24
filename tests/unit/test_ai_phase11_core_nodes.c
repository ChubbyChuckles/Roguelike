#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include "../../src/ai/nodes/basic_nodes.h"
#include <assert.h>
#include <string.h>

/* Helper leaves producing configurable status sequence */
static RogueBTStatus seq_status_fn(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
    int* idx = (int*) n->user_data;
    static const RogueBTStatus order[3] = {ROGUE_BT_RUNNING, ROGUE_BT_FAILURE, ROGUE_BT_SUCCESS};
    RogueBTStatus st = order[*idx % 3];
    (*idx)++;
    return st;
}

static void test_selector_short_circuit()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    /* First child succeeds immediately -> selector stops */
    RogueBTNode* root = rogue_bt_selector("sel");
    RogueBTNode* succ = rogue_bt_leaf_always_success("A");
    int idx = 0;
    RogueBTNode* seq = rogue_bt_node_create("seq", 0, seq_status_fn);
    seq->user_data = &idx; /* would run but should be skipped */
    rogue_bt_node_add_child(root, succ);
    rogue_bt_node_add_child(root, seq);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(root);
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS && idx == 0); /* second child never ticked */
    rogue_behavior_tree_destroy(tree);
}

static void test_sequence_short_circuit()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    RogueBTNode* root = rogue_bt_sequence("seq");
    RogueBTNode* fail = rogue_bt_leaf_always_failure("F");
    int idx = 0;
    RogueBTNode* seq = rogue_bt_node_create("leaf", 0, seq_status_fn);
    seq->user_data = &idx; /* should not tick */
    rogue_bt_node_add_child(root, fail);
    rogue_bt_node_add_child(root, seq);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(root);
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE && idx == 0);
    rogue_behavior_tree_destroy(tree);
}

static float score_one(struct RogueBlackboard* bb, void* ud)
{
    (void) bb;
    return (float) (uintptr_t) ud;
}
static void test_utility_tie_break()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    RogueBTNode* util = rogue_bt_utility_selector("util");
    RogueBTNode* a = rogue_bt_leaf_always_success("A");
    RogueBTNode* b = rogue_bt_leaf_always_success("B");
    RogueUtilityScorer s1 = {score_one, (void*) (uintptr_t) 5, "s1"};
    RogueUtilityScorer s2 = {score_one, (void*) (uintptr_t) 5, "s2"};
    assert(rogue_bt_utility_set_child_scorer(util, a, s1));
    assert(rogue_bt_utility_set_child_scorer(util, b, s2));
    RogueBehaviorTree* tree = rogue_behavior_tree_create(util);
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    (void) st; /* success either way */
    /* Tie should pick first child (A) so active path serialization contains A only */
    char path[128];
    int n = rogue_behavior_tree_serialize_active_path(tree, path, sizeof path);
    assert(n > 0 && strstr(path, "A"));
    rogue_behavior_tree_destroy(tree);
}

/* Parallel mix: one running, rest success => running; any failure => failure */
static RogueBTStatus leaf_run(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    (void) bb;
    (void) dt;
    return ROGUE_BT_RUNNING;
}
static RogueBTStatus leaf_succ(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    (void) bb;
    (void) dt;
    return ROGUE_BT_SUCCESS;
}
static RogueBTStatus leaf_fail(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    (void) bb;
    (void) dt;
    return ROGUE_BT_FAILURE;
}
static void test_parallel_mix()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    RogueBTNode* par = rogue_bt_parallel("par");
    RogueBTNode* r = rogue_bt_node_create("r", 0, leaf_run);
    RogueBTNode* s = rogue_bt_node_create("s", 0, leaf_succ);
    rogue_bt_node_add_child(par, r);
    rogue_bt_node_add_child(par, s);
    RogueBehaviorTree* t = rogue_behavior_tree_create(par);
    assert(rogue_behavior_tree_tick(t, &bb, 0.01f) == ROGUE_BT_RUNNING);
    rogue_behavior_tree_destroy(t);
    par = rogue_bt_parallel("par2");
    r = rogue_bt_node_create("f", 0, leaf_fail);
    s = rogue_bt_node_create("s", 0, leaf_succ);
    rogue_bt_node_add_child(par, r);
    rogue_bt_node_add_child(par, s);
    t = rogue_behavior_tree_create(par);
    assert(rogue_behavior_tree_tick(t, &bb, 0.01f) == ROGUE_BT_FAILURE);
    rogue_behavior_tree_destroy(t);
}

/* Cooldown boundary: ensure success resets timer & subsequent ticks before duration fail */
static void test_cooldown_boundary()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    rogue_bb_set_timer(&bb, "cool", 0.0f);
    rogue_bb_set_bool(&bb, "flag", true);
    RogueBTNode* atk = rogue_bt_action_attack_melee("atk", "flag", "cool", 0.2f);
    RogueBTNode* cd = rogue_bt_decorator_cooldown("cd", atk, "cool", 0.2f);
    RogueBehaviorTree* t = rogue_behavior_tree_create(cd);
    assert(rogue_behavior_tree_tick(t, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    for (int i = 0; i < 5; i++)
    {
        assert(rogue_behavior_tree_tick(t, &bb, 0.05f) == ROGUE_BT_FAILURE);
    }
    /* advance boundary exactly */
    rogue_behavior_tree_tick(t, &bb, 0.10f); /* bring timer >= cooldown */
    assert(rogue_behavior_tree_tick(t, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    rogue_behavior_tree_destroy(t);
}

/* Retry decorator: child fails N times then success; ensure attempts reset after success */
static RogueBTStatus fail_then_succeed(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
    int* counter = (int*) n->user_data;
    if ((*counter)++ < 2)
        return ROGUE_BT_FAILURE;
    return ROGUE_BT_SUCCESS;
}
static void test_retry_reset()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    int counter = 0;
    RogueBTNode* leaf = rogue_bt_node_create("ft", 0, fail_then_succeed);
    leaf->user_data = &counter;
    RogueBTNode* retry = rogue_bt_decorator_retry("retry", leaf, 3);
    RogueBehaviorTree* t = rogue_behavior_tree_create(retry);
    RogueBTStatus st;
    st = rogue_behavior_tree_tick(t, &bb, 0.01f);
    assert(st == ROGUE_BT_RUNNING);
    st = rogue_behavior_tree_tick(t, &bb, 0.01f);
    assert(st == ROGUE_BT_RUNNING);
    st = rogue_behavior_tree_tick(t, &bb, 0.01f);
    assert(st == ROGUE_BT_SUCCESS);
    /* Counter now 3; reset path: run again should attempt again */
    counter = 0;
    st = rogue_behavior_tree_tick(t, &bb, 0.01f);
    assert(st == ROGUE_BT_RUNNING);
    rogue_behavior_tree_destroy(t);
}

int main(void)
{
    test_selector_short_circuit();
    test_sequence_short_circuit();
    test_utility_tie_break();
    test_parallel_mix();
    test_cooldown_boundary();
    test_retry_reset();
    return 0;
}
