/* Enemy AI Behavior Tree Integration (Phase: Enemy Integration Feature Flag)
   Builds a simple behavior tree for an enemy when feature flag enabled.
   Initial tree: MoveToPlayer action moving toward player position each tick.
*/
#include <stdlib.h>
#include <math.h>
#include "entities/enemy.h"
#include "core/app_state.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/nodes/basic_nodes.h"
#include "ai/nodes/advanced_nodes.h"

typedef struct EnemyAIBlackboard {
    RogueBlackboard bb;
    const char* player_pos_key;
    const char* agent_pos_key;
    const char* agent_facing_key;
    const char* move_reached_flag;
} EnemyAIBlackboard;

static void enemy_ai_sync_bb(EnemyAIBlackboard* ebb, RogueEnemy* e){
    rogue_bb_set_vec2(&ebb->bb, ebb->agent_pos_key, e->base.pos.x, e->base.pos.y);
    rogue_bb_set_vec2(&ebb->bb, ebb->player_pos_key, g_app.player.base.pos.x, g_app.player.base.pos.y);
    float dx = g_app.player.base.pos.x - e->base.pos.x; float dy = g_app.player.base.pos.y - e->base.pos.y; float len = sqrtf(dx*dx+dy*dy); if(len<0.0001f){ dx=1; dy=0; len=1; } dx/=len; dy/=len;
    rogue_bb_set_vec2(&ebb->bb, ebb->agent_facing_key, dx, dy);
}

static RogueBehaviorTree* enemy_ai_build_bt(EnemyAIBlackboard* ebb){
    RogueBTNode* move = rogue_bt_action_move_to("MoveToPlayer", ebb->player_pos_key, ebb->agent_pos_key, 2.0f, ebb->move_reached_flag);
    return rogue_behavior_tree_create(move);
}

void rogue_enemy_ai_bt_enable(RogueEnemy* e){
    if(!e || e->ai_bt_enabled) return;
    e->ai_bt_enabled = 1;
    EnemyAIBlackboard* ebb = (EnemyAIBlackboard*)calloc(1,sizeof(EnemyAIBlackboard));
    rogue_bb_init(&ebb->bb);
    ebb->player_pos_key = "player_pos";
    ebb->agent_pos_key = "agent_pos";
    ebb->agent_facing_key = "agent_facing";
    ebb->move_reached_flag = "move_reached";
    enemy_ai_sync_bb(ebb, e);
    e->ai_tree = enemy_ai_build_bt(ebb);
    e->ai_bt_state = ebb;
}

void rogue_enemy_ai_bt_disable(RogueEnemy* e){
    if(!e || !e->ai_bt_enabled) return;
    e->ai_bt_enabled = 0;
    if(e->ai_tree){ rogue_behavior_tree_destroy(e->ai_tree); e->ai_tree=NULL; }
    if(e->ai_bt_state){ free(e->ai_bt_state); e->ai_bt_state=NULL; }
}

void rogue_enemy_ai_bt_tick(RogueEnemy* e, float dt){
    if(!e || !e->ai_bt_enabled || !e->ai_tree) return;
    EnemyAIBlackboard* ebb = (EnemyAIBlackboard*)e->ai_bt_state;
    if(!ebb) return;
    enemy_ai_sync_bb(ebb, e);
    rogue_behavior_tree_tick(e->ai_tree, &ebb->bb, dt);
    RogueBBVec2 agent; if(rogue_bb_get_vec2(&ebb->bb, ebb->agent_pos_key, &agent)){ e->base.pos.x = agent.x; e->base.pos.y = agent.y; }
}
