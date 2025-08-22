#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/nodes/advanced_nodes.h"
#include "ai/nodes/basic_nodes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    // 7.1 Squad ids
    RogueBTNode* n_ids = rogue_bt_tactical_squad_set_ids("ids", "squad_id", 42, "member_index", 2,
                                                         "member_total", 6);
    RogueBehaviorTree* t_ids = rogue_behavior_tree_create(n_ids);
    assert(rogue_behavior_tree_tick(t_ids, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    int squad_id = 0, member_index = 0, member_total = 0;
    assert(rogue_bb_get_int(&bb, "squad_id", &squad_id) && squad_id == 42);
    assert(rogue_bb_get_int(&bb, "member_index", &member_index) && member_index == 2);
    assert(rogue_bb_get_int(&bb, "member_total", &member_total) && member_total == 6);
    rogue_behavior_tree_destroy(t_ids);

    // 7.2 Role assign (fallback by modulo)
    RogueBTNode* n_role = rogue_bt_tactical_role_assign("role", "role", "member_index",
                                                        "member_total", NULL, NULL, NULL);
    RogueBehaviorTree* t_role = rogue_behavior_tree_create(n_role);
    assert(rogue_behavior_tree_tick(t_role, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    int role = -1;
    assert(rogue_bb_get_int(&bb, "role", &role) && role == (2 % 3));
    rogue_behavior_tree_destroy(t_role);

    // 7.2 Role assign (weights dominate)
    rogue_bb_set_float(&bb, "w_b", 0.1f);
    rogue_bb_set_float(&bb, "w_h", 0.7f);
    rogue_bb_set_float(&bb, "w_s", 0.4f);
    RogueBTNode* n_role_w = rogue_bt_tactical_role_assign("rolew", "rolew", "member_index",
                                                          "member_total", "w_b", "w_h", "w_s");
    RogueBehaviorTree* t_role_w = rogue_behavior_tree_create(n_role_w);
    assert(rogue_behavior_tree_tick(t_role_w, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    int rolew = -1;
    assert(rogue_bb_get_int(&bb, "rolew", &rolew) && rolew == 1);
    rogue_behavior_tree_destroy(t_role_w);

    // 7.3 Surround slot assigns point on circle
    RogueBBVec2 tgt = {10.0f, 20.0f};
    rogue_bb_set_vec2(&bb, "tgt", tgt.x, tgt.y);
    RogueBTNode* n_sur = rogue_bt_tactical_surround_assign_slot("sur", "tgt", "member_index",
                                                                "member_total", 5.0f, "slot");
    RogueBehaviorTree* t_sur = rogue_behavior_tree_create(n_sur);
    assert(rogue_behavior_tree_tick(t_sur, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    RogueBBVec2 slot;
    assert(rogue_bb_get_vec2(&bb, "slot", &slot));
    float dx = slot.x - tgt.x;
    float dy = slot.y - tgt.y;
    float r2 = dx * dx + dy * dy;
    assert(r2 > 24.0f && r2 < 26.0f);
    rogue_behavior_tree_destroy(t_sur);

    // 7.4 Retreat condition
    rogue_bb_set_float(&bb, "hp", 0.25f);
    rogue_bb_set_int(&bb, "deaths", 1);
    RogueBTNode* n_ret = rogue_bt_condition_should_retreat("ret", "hp", 0.3f, "deaths", 2);
    RogueBehaviorTree* t_ret = rogue_behavior_tree_create(n_ret);
    assert(rogue_behavior_tree_tick(t_ret, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    rogue_behavior_tree_destroy(t_ret);

    // 7.5 Stagger decorator (index 2 -> needs 2*base delay)
    RogueBTNode* child_ok = rogue_bt_leaf_always_success("ok");
    RogueBTNode* n_stag =
        rogue_bt_decorator_stagger_by_index("stag", child_ok, "member_index", "stag_timer", 0.02f);
    RogueBehaviorTree* t_stag = rogue_behavior_tree_create(n_stag);
    // Tick 1 -> timer should be ~0.016, still RUNNING
    assert(rogue_behavior_tree_tick(t_stag, &bb, 0.016f) == ROGUE_BT_RUNNING);
    float tval = -1.0f;
    assert(rogue_bb_get_timer(&bb, "stag_timer", &tval));
    assert(tval > 0.015f && tval < 0.0175f);
    // Tick 2 -> timer ~0.032, still RUNNING (need >= 0.04 for index=2)
    assert(rogue_behavior_tree_tick(t_stag, &bb, 0.016f) == ROGUE_BT_RUNNING);
    assert(rogue_bb_get_timer(&bb, "stag_timer", &tval));
    assert(tval > 0.031f && tval < 0.0335f);
    assert(rogue_behavior_tree_tick(t_stag, &bb, 0.016f) == ROGUE_BT_SUCCESS);
    // Timer should reset on success and run again next frame
    assert(rogue_behavior_tree_tick(t_stag, &bb, 0.016f) == ROGUE_BT_RUNNING);
    rogue_behavior_tree_destroy(t_stag);

    puts("AI_PHASE7_GROUP_TACTICS_OK");
    return 0;
}
