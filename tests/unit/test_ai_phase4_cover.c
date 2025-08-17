#include <assert.h>
#include <math.h>
#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"

int main(void){
    RogueBlackboard bb; rogue_bb_init(&bb);
    /* Player at origin, obstacle between player and intended cover point */
    rogue_bb_set_vec2(&bb,"player_pos",0.0f,0.0f);
    rogue_bb_set_vec2(&bb,"agent_pos",5.0f,0.0f);
    rogue_bb_set_vec2(&bb,"rock_pos",2.5f,0.0f);
    RogueBTNode* cover = rogue_bt_tactical_cover_seek("cover","player_pos","agent_pos","rock_pos","cover_point","in_cover",0.6f,6.0f);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(cover);
    RogueBTStatus st=ROGUE_BT_RUNNING; float elapsed=0.0f; while(st==ROGUE_BT_RUNNING && elapsed<5.0f){ st=rogue_behavior_tree_tick(tree,&bb,0.1f); elapsed+=0.1f; }
    assert(st==ROGUE_BT_SUCCESS);
    bool in_cover=false; rogue_bb_get_bool(&bb,"in_cover",&in_cover); assert(in_cover);
    RogueBBVec2 cp; assert(rogue_bb_get_vec2(&bb,"cover_point",&cp));
    /* Cover point should lie just behind obstacle relative to player (x slightly > obstacle.x) */
    assert(cp.x > 2.5f);
    rogue_behavior_tree_destroy(tree);
    return 0;
}
