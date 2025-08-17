#include <assert.h>
#include <stdio.h>
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/nodes/basic_nodes.h"

static void test_blackboard_basic() {
    RogueBlackboard bb; rogue_bb_init(&bb);
    assert(rogue_bb_set_bool(&bb, "can_see_player", true));
    bool out=false; assert(rogue_bb_get_bool(&bb, "can_see_player", &out));
    assert(out == true);
}

static void test_bt_selector_sequence() {
    RogueBlackboard bb; rogue_bb_init(&bb);
    RogueBTNode* root = rogue_bt_selector("root_selector");
    RogueBTNode* seq = rogue_bt_sequence("seq");
    RogueBTNode* cond = rogue_bt_leaf_check_bool("see_player?", "see", true);
    RogueBTNode* succ = rogue_bt_leaf_always_success("fallback_success");

    rogue_bt_node_add_child(root, seq);
    rogue_bt_node_add_child(root, succ);
    rogue_bt_node_add_child(seq, cond); // sequence has only one child for simplicity

    RogueBehaviorTree* tree = rogue_behavior_tree_create(root);
    // First tick: condition fails (no key), selector should evaluate seq->FAIL then fallback success returns SUCCESS
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);

    // Set the key so condition passes; still success overall
    assert(rogue_bb_set_bool(&bb, "see", true));
    st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);

    rogue_behavior_tree_destroy(tree);
}

int main() {
    test_blackboard_basic();
    test_bt_selector_sequence();
    printf("[test_ai_behavior_tree] All tests passed.\n");
    return 0;
}
