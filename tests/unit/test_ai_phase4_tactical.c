#include <assert.h>
#include <math.h>
#include <string.h>
#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/basic_nodes.h"
#include "../../src/ai/nodes/advanced_nodes.h"

static void test_strafe_action(){
    RogueBlackboard bb; rogue_bb_init(&bb);
    rogue_bb_set_vec2(&bb,"player_pos",5.0f,0.0f);
    rogue_bb_set_vec2(&bb,"agent_pos",0.0f,0.0f);
    rogue_bb_set_bool(&bb,"strafe_left",false);
    RogueBTNode* strafe = rogue_bt_action_strafe("strafe","player_pos","agent_pos","strafe_left",4.0f,0.5f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(strafe);
    float elapsed=0.0f; RogueBTStatus st=ROGUE_BT_RUNNING; while(st==ROGUE_BT_RUNNING && elapsed < 2.0f){ st=rogue_behavior_tree_tick(tree,&bb,0.1f); elapsed+=0.1f; }
    assert(st==ROGUE_BT_SUCCESS);
    bool left=false; rogue_bb_get_bool(&bb,"strafe_left",&left); assert(left==true); /* flipped after completion */
    rogue_behavior_tree_destroy(tree);
}

static void test_flank_and_regroup(){
    RogueBlackboard bb; rogue_bb_init(&bb);
    rogue_bb_set_vec2(&bb,"player_pos",5.0f,0.0f);
    rogue_bb_set_vec2(&bb,"agent_pos",0.0f,0.0f);
    RogueBTNode* flank = rogue_bt_tactical_flank_attempt("flank","player_pos","agent_pos","flank_point",2.0f);
    RogueBTNode* flank_tree_root = flank;
    RogueBehaviorTree* flank_tree = rogue_behavior_tree_create(flank_tree_root);
    assert(rogue_behavior_tree_tick(flank_tree,&bb,0.016f)==ROGUE_BT_SUCCESS);
    RogueBBVec2 flank_pt; assert(rogue_bb_get_vec2(&bb,"flank_point",&flank_pt));
    /* Expect flank point offset roughly perpendicular (y positive since player at +x) */
    assert(fabsf(flank_pt.y) > 0.1f);
    rogue_behavior_tree_destroy(flank_tree);

    /* Regroup movement toward target */
    rogue_bb_set_vec2(&bb,"regroup_pos",3.0f,0.0f);
    rogue_bb_set_vec2(&bb,"agent_pos",0.0f,0.0f);
    RogueBTNode* regroup = rogue_bt_tactical_regroup("regroup","regroup_pos","agent_pos",6.0f);
    RogueBehaviorTree* regroup_tree = rogue_behavior_tree_create(regroup);
    float elapsed=0.0f; RogueBTStatus rs=ROGUE_BT_RUNNING; while(rs==ROGUE_BT_RUNNING && elapsed < 3.0f){ rs=rogue_behavior_tree_tick(regroup_tree,&bb,0.1f); elapsed+=0.1f; }
    assert(rs==ROGUE_BT_SUCCESS);
    rogue_behavior_tree_destroy(regroup_tree);
}

static void test_cover_seek(){
    RogueBlackboard bb; rogue_bb_init(&bb);
    /* Provide required positional keys for cover seek (player, agent, obstacle) plus output keys */
    rogue_bb_set_vec2(&bb,"player_pos",5.0f,0.0f);
    rogue_bb_set_vec2(&bb,"agent_pos",0.0f,0.0f);
    rogue_bb_set_vec2(&bb,"rock_pos",2.5f,0.0f);
    rogue_bb_set_bool(&bb,"in_cover",false);
    RogueBTNode* cover = rogue_bt_tactical_cover_seek("cover","player_pos","agent_pos","rock_pos","cover_point","in_cover",0.6f,6.0f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(cover);
    assert(rogue_behavior_tree_tick(tree,&bb,0.016f)==ROGUE_BT_SUCCESS);
    bool in_cover=false; rogue_bb_get_bool(&bb,"in_cover",&in_cover); assert(in_cover);
    rogue_behavior_tree_destroy(tree);
}

int main(void){
    test_strafe_action();
    test_flank_and_regroup();
    test_cover_seek();
    return 0;
}
