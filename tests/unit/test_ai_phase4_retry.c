#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include "../../src/ai/nodes/basic_nodes.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* A leaf that fails N-1 times then succeeds once, tracking attempts via blackboard int */
typedef struct
{
    const char* counter_key;
    int fail_count_before_success; /* number of failures before one success */
} FlakyConfig;

static RogueBTStatus tick_flaky(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    FlakyConfig* cfg = (FlakyConfig*) node->user_data;
    int val = 0;
    rogue_bb_get_int(bb, cfg->counter_key, &val);
    rogue_bb_set_int(bb, cfg->counter_key, val + 1, ROGUE_BB_WRITE_SET);
    if (val < cfg->fail_count_before_success)
    {
        return ROGUE_BT_FAILURE;
    }
    return ROGUE_BT_SUCCESS;
}

static RogueBTNode* make_flaky(const char* name, const char* counter_key, int fail_before_success)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_flaky);
    FlakyConfig* cfg = (FlakyConfig*) calloc(1, sizeof(FlakyConfig));
    cfg->counter_key = counter_key;
    cfg->fail_count_before_success = fail_before_success;
    n->user_data = cfg; /* Intentionally not freed (test process lifetime) */
    return n;
}

static void test_retry_success_path()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    rogue_bb_set_int(&bb, "attempts", 0);
    /* Child will fail 2 times then succeed on 3rd tick */
    RogueBTNode* flaky = make_flaky("flaky", "attempts", 2);
    RogueBTNode* retry = rogue_bt_decorator_retry("retry", flaky, 5); /* allow up to 5 attempts */
    RogueBehaviorTree* tree = rogue_behavior_tree_create(retry);
    RogueBTStatus st;
    int ticks = 0;
    do
    {
        st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
        ticks++;
        assert(ticks < 10);
    } while (st == ROGUE_BT_RUNNING);
    assert(st == ROGUE_BT_SUCCESS);
    int attempts = 0;
    rogue_bb_get_int(&bb, "attempts", &attempts);
    assert(attempts == 3); /* two failures + one success */
    rogue_behavior_tree_destroy(tree);
}

static void test_retry_exhaustion()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    rogue_bb_set_int(&bb, "attempts2", 0);
    /* Child always fails (set large fail_before_success); retry max attempts should exhaust */
    RogueBTNode* flaky = make_flaky("flaky2", "attempts2", 100);
    RogueBTNode* retry = rogue_bt_decorator_retry("retry2", flaky, 4); /* allow 4 attempts */
    RogueBehaviorTree* tree = rogue_behavior_tree_create(retry);
    RogueBTStatus st;
    int ticks = 0;
    do
    {
        st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
        ticks++;
        assert(ticks < 10);
    } while (st == ROGUE_BT_RUNNING);
    assert(st == ROGUE_BT_FAILURE);
    int attempts = 0;
    rogue_bb_get_int(&bb, "attempts2", &attempts);
    assert(attempts == 4); /* max attempts consumed */
    rogue_behavior_tree_destroy(tree);
}

int main(void)
{
    test_retry_success_path();
    test_retry_exhaustion();
    return 0;
}
