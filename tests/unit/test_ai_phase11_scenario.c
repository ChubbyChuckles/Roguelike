#include "ai/core/ai_trace.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/nodes/advanced_nodes.h"
#include "ai/nodes/basic_nodes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Scenario: patrol waypoints -> detect player -> chase -> lose track -> resume patrol */

typedef struct PatrolData
{
    int index;
    int count;
    RogueBBVec2 points[4];
    const char* agent_pos_key;
    const char* out_reached;
} PatrolData;
static RogueBTStatus tick_patrol(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    PatrolData* d = (PatrolData*) n->user_data;
    RogueBBVec2 cur;
    rogue_bb_get_vec2(bb, d->agent_pos_key, &cur);
    RogueBBVec2 target = d->points[d->index];
    float dx = target.x - cur.x, dy = target.y - cur.y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 < 0.04f)
    {
        d->index = (d->index + 1) % d->count;
        rogue_bb_set_bool(bb, d->out_reached, true);
        return ROGUE_BT_SUCCESS;
    }
    float dist = sqrtf(dist2);
    cur.x += (dx / dist) * 2.f * dt;
    cur.y += (dy / dist) * 2.f * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, cur.x, cur.y);
    rogue_bb_set_bool(bb, d->out_reached, false);
    return ROGUE_BT_RUNNING;
}
static RogueBTNode* node_patrol(const char* name, const char* agent_pos_key,
                                const char* out_reached)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_patrol);
    PatrolData* d = (PatrolData*) calloc(1, sizeof(PatrolData));
    d->agent_pos_key = agent_pos_key;
    d->out_reached = out_reached;
    d->count = 4;
    d->points[0] = (RogueBBVec2){0, 0};
    d->points[1] = (RogueBBVec2){3, 0};
    d->points[2] = (RogueBBVec2){3, 3};
    d->points[3] = (RogueBBVec2){0, 3};
    n->user_data = d;
    return n;
}

/* Condition: player visible (bool key set) */
static RogueBTStatus tick_cond_visible(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    (void) dt;
    bool v = false;
    if (!rogue_bb_get_bool(bb, "player_visible", &v))
        return ROGUE_BT_FAILURE;
    return v ? ROGUE_BT_SUCCESS : ROGUE_BT_FAILURE;
}
static RogueBTNode* cond_visible() { return rogue_bt_node_create("vis", 0, tick_cond_visible); }

/* Action: chase player (move directly) */
static RogueBTStatus tick_chase(RogueBTNode* n, RogueBlackboard* bb, float dt)
{
    (void) n;
    RogueBBVec2 player, agent;
    if (!rogue_bb_get_vec2(bb, "player_pos", &player))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, "agent_pos", &agent))
        return ROGUE_BT_FAILURE;
    float dx = player.x - agent.x, dy = player.y - agent.y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 < 0.09f)
    {
        return ROGUE_BT_SUCCESS;
    }
    float dist = sqrtf(dist2);
    agent.x += (dx / dist) * 3.f * dt;
    agent.y += (dy / dist) * 3.f * dt;
    rogue_bb_set_vec2(bb, "agent_pos", agent.x, agent.y);
    return ROGUE_BT_RUNNING;
}
static RogueBTNode* action_chase() { return rogue_bt_node_create("chase", 0, tick_chase); }

/* Decorator: while visible chase else failure to fallback to patrol */

int main(void)
{
    RogueBlackboard bb;
    rogue_bb_init(&bb);
    rogue_bb_set_vec2(&bb, "agent_pos", 0, 0);
    rogue_bb_set_vec2(&bb, "player_pos", 5, 0);
    RogueBTNode* root = rogue_bt_selector("root");
    /* Branch 1: If visible -> chase */
    RogueBTNode* seq = rogue_bt_sequence("seq_vis");
    rogue_bt_node_add_child(seq, cond_visible());
    rogue_bt_node_add_child(seq, action_chase());
    RogueBTNode* patrol = node_patrol("patrol", "agent_pos", "patrol_step");
    rogue_bt_node_add_child(root, seq);
    rogue_bt_node_add_child(root, patrol);
    RogueBehaviorTree* tree = rogue_behavior_tree_create(root);
    RogueAITraceBuffer trace;
    rogue_ai_trace_init(&trace);
    char path[256];
    /* Phase 1: patrol (not visible) */
    for (int i = 0; i < 60; i++)
    {
        rogue_behavior_tree_tick(tree, &bb, 0.05f);
        int n = rogue_behavior_tree_serialize_active_path(tree, path, sizeof path);
        (void) n;
    }
    RogueBBVec2 pos;
    rogue_bb_get_vec2(&bb, "agent_pos", &pos);
    assert(pos.x != 0.f || pos.y != 0.f); /* moved */
    /* Make player visible -> chase */
    rogue_bb_set_bool(&bb, "player_visible", true);
    for (int i = 0; i < 40; i++)
    {
        rogue_behavior_tree_tick(tree, &bb, 0.05f);
    }
    RogueBBVec2 pos2;
    rogue_bb_get_vec2(&bb, "agent_pos", &pos2);
    float dpos = (pos2.x - pos.x) * (pos2.x - pos.x) + (pos2.y - pos.y) * (pos2.y - pos.y);
    assert(dpos > 0.01f);
    /* Lose visibility -> decay via manual toggle simulate TTL expiry */
    rogue_bb_set_bool(&bb, "player_visible", false);
    RogueBBVec2 before;
    rogue_bb_get_vec2(&bb, "agent_pos", &before);
    for (int i = 0; i < 40; i++)
    {
        rogue_behavior_tree_tick(tree, &bb, 0.05f);
    }
    RogueBBVec2 after;
    rogue_bb_get_vec2(&bb, "agent_pos",
                      &after); /* Should have resumed patrol movement (non-zero displacement) */
    float disp =
        (after.x - before.x) * (after.x - before.x) + (after.y - before.y) * (after.y - before.y);
    assert(disp > 0.0001f);
    rogue_behavior_tree_destroy(tree);
    printf("AI_PHASE11_SCENARIO_OK patrol->chase->patrol cycle complete\n");
    return 0;
}
