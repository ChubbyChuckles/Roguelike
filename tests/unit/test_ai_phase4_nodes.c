#include <assert.h>
#include <math.h>
#include <string.h>
#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/basic_nodes.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include "../../src/ai/util/utility_scorer.h"

/* Scorer helpers */
static float score_low(struct RogueBlackboard* bb, void* ud){ (void)bb; (void)ud; return 1.0f; }
static float score_high(struct RogueBlackboard* bb, void* ud){ (void)bb; (void)ud; return 5.0f; }

static void test_utility_selector(){
    RogueBlackboard bb; rogue_bb_init(&bb);
    RogueBTNode* util = rogue_bt_utility_selector("util");
    RogueBTNode* succ = rogue_bt_leaf_always_success("succ");
    RogueBTNode* fail = rogue_bt_leaf_always_failure("fail");
    RogueUtilityScorer low = { score_low, NULL, "low" };
    RogueUtilityScorer high = { score_high, NULL, "high" };
    assert(rogue_bt_utility_set_child_scorer(util, fail, low));
    assert(rogue_bt_utility_set_child_scorer(util, succ, high));
    RogueBehaviorTree* tree = rogue_behavior_tree_create(util);
    RogueBTStatus st = rogue_behavior_tree_tick(tree, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS); /* high scorer picks success child */
    rogue_behavior_tree_destroy(tree); /* destroys util and its children */
}

static void test_conditions_actions(){
    RogueBlackboard bb; rogue_bb_init(&bb);
    rogue_bb_set_vec2(&bb, "player_pos", 5.0f, 0.0f);
    rogue_bb_set_vec2(&bb, "agent_pos", 0.0f, 0.0f);
    rogue_bb_set_vec2(&bb, "agent_facing", 1.0f, 0.0f);
    RogueBTNode* visible = rogue_bt_condition_player_visible("vis", "player_pos", "agent_pos", "agent_facing", 140.0f, 20.0f);
    RogueBTNode* seq = rogue_bt_sequence("seq");
    rogue_bt_node_add_child(seq, visible);
    RogueBTNode* move = rogue_bt_action_move_to("move", "player_pos", "agent_pos", 10.0f, "reached");
    rogue_bt_node_add_child(seq, move);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(seq);
    int max_ticks = 200; int reached = 0; for(int i=0;i<max_ticks;i++){
        RogueBTStatus st = rogue_behavior_tree_tick(tree,&bb,0.05f);
        bool r=false; rogue_bb_get_bool(&bb, "reached", &r); if(r){ reached=1; break; }
        (void)st; /* ignore running state until reached */
    }
    assert(reached==1);
    rogue_behavior_tree_destroy(tree); /* children destroyed recursively */
}

static void test_cooldown_retry(){
    RogueBlackboard bb; rogue_bb_init(&bb);
    rogue_bb_set_timer(&bb, "cool_timer", 0.0f);
    rogue_bb_set_bool(&bb, "in_range", true);
    RogueBTNode* attack = rogue_bt_action_attack_melee("atk", "in_range", "cool_timer", 0.3f);
    RogueBTNode* cd = rogue_bt_decorator_cooldown("cd", attack, "cool_timer", 0.3f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(cd);
    /* First tick triggers attack (success + resets timer) */
    assert(rogue_behavior_tree_tick(tree,&bb,0.016f) == ROGUE_BT_SUCCESS);
    /* Subsequent ticks before cooldown should fail (cooldown enforcing) */
    for(int i=0;i<5;i++){ assert(rogue_behavior_tree_tick(tree,&bb,0.05f) == ROGUE_BT_FAILURE); }
    /* Advance enough time */
    for(int i=0;i<10;i++){ rogue_behavior_tree_tick(tree,&bb,0.05f); }
    /* Attack again should succeed once timer >= cooldown */
    assert(rogue_behavior_tree_tick(tree,&bb,0.016f) == ROGUE_BT_SUCCESS);
    rogue_behavior_tree_destroy(tree);
    /* child freed by tree destroy */
}

int main(void){
    test_utility_selector();
    test_conditions_actions();
    test_cooldown_retry();
    return 0;
}
