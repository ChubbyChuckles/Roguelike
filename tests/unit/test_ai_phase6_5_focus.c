#include "../../src/ai/core/behavior_tree.h"
#include "../../src/ai/core/blackboard.h"
#include "../../src/ai/nodes/advanced_nodes.h"
#include <assert.h>
#include <string.h>

/* Test focus broadcast/decay: leader sets flag/pos and TTL resets; decay clears after TTL. */
static void test_focus_broadcast_and_decay()
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);

    const char* kThreat = "threat";
    const char* kTgtPos = "tpos";
    const char* kFlag = "gfocus";
    const char* kPos = "gpos";
    const char* kTTL = "gttl";

    rogue_bb_set_float(&bb, kThreat, 0.0f);
    rogue_bb_set_vec2(&bb, kTgtPos, 10.0f, 5.0f);
    rogue_bb_set_bool(&bb, kFlag, false);
    rogue_bb_set_vec2(&bb, kPos, 0.0f, 0.0f);
    rogue_bb_set_timer(&bb, kTTL, 999.0f); /* ensure reset happens */

    RogueBTNode* broadcast = rogue_bt_tactical_focus_broadcast_if_leader(
        "broadcast", kThreat, 2.0f, kTgtPos, kFlag, kPos, kTTL);
    RogueBTNode* decay = rogue_bt_tactical_focus_decay("decay", kFlag, kTTL, 0.25f);

    RogueBehaviorTree* bt = rogue_behavior_tree_create(broadcast);
    assert(bt);

    /* Not leader -> no broadcast */
    RogueBTStatus st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_FAILURE);
    bool flag = true;
    rogue_bb_get_bool(&bb, kFlag, &flag);
    assert(flag == false);

    /* Become leader, broadcast should set flag/pos and reset ttl timer */
    rogue_bb_set_float(&bb, kThreat, 3.0f);
    st = rogue_behavior_tree_tick(bt, &bb, 0.016f);
    assert(st == ROGUE_BT_SUCCESS);
    rogue_bb_get_bool(&bb, kFlag, &flag);
    assert(flag == true);
    RogueBBVec2 gpos;
    rogue_bb_get_vec2(&bb, kPos, &gpos);
    assert(gpos.x == 10.0f && gpos.y == 5.0f);
    float t = 1.0f;
    rogue_bb_get_timer(&bb, kTTL, &t);
    assert(t == 0.0f);

    rogue_behavior_tree_destroy(bt);

    /* Now test decay */
    bt = rogue_behavior_tree_create(decay);

    /* Immediately after broadcast, ttl=0, active => success */
    st = rogue_behavior_tree_tick(bt, &bb, 0.10f);
    assert(st == ROGUE_BT_SUCCESS);
    /* After another tick pushing past ttl, flag should clear and FAILURE */
    st = rogue_behavior_tree_tick(bt, &bb, 0.20f);
    assert(st == ROGUE_BT_FAILURE);
    rogue_bb_get_bool(&bb, kFlag, &flag);
    assert(flag == false);

    rogue_behavior_tree_destroy(bt);
}

int main()
{
    test_focus_broadcast_and_decay();
    return 0;
}
