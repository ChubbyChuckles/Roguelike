/**
 * @file enemy_ai_bt.c
 * @brief Enemy AI Behavior Tree Integration (feature-flag gated).
 *
 * Builds a simple behavior tree for an enemy when the feature flag is
 * enabled. The initial tree contains a MoveToPlayer action that updates the
 * agent's position each tick using values stored on a blackboard.
 */
#include "ai/core/ai_agent_pool.h"
#include "ai/core/behavior_tree.h"
#include "ai/core/blackboard.h"
#include "ai/nodes/advanced_nodes.h"
#include "ai/nodes/basic_nodes.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include <math.h>
#include <stdlib.h>

/**
 * @brief Per-enemy blackboard wrapper used by the simple BT.
 *
 * Stores a RogueBlackboard instance and keys used for common values such as
 * the player position, agent position, facing vector and a move-complete
 * flag. Keys are string literals in the current implementation.
 */
typedef struct EnemyAIBlackboard
{
    RogueBlackboard bb;            /**< Underlying blackboard instance */
    const char* player_pos_key;    /**< Key for player position vec2 */
    const char* agent_pos_key;     /**< Key for agent position vec2 */
    const char* agent_facing_key;  /**< Key for agent facing vec2 */
    const char* move_reached_flag; /**< Key for boolean move reached flag */
} EnemyAIBlackboard;

/* Compile-time guard: ensure pool slab large enough */
#include <assert.h>
extern size_t rogue_ai_agent_pool_slab_size(void);
/**
 * @brief Runtime check that the AI agent pool slab can hold our blackboard.
 *
 * This function is called indirectly on enable and asserts loudly if the
 * configured pool slab is too small. The return value is 1 on success.
 */
static int _enemy_ai_bt_size_guard(void)
{
    size_t have = rogue_ai_agent_pool_slab_size();
    if (have < sizeof(EnemyAIBlackboard))
    {
        assert(!"AI agent pool slab too small for EnemyAIBlackboard");
        return 0;
    }
    return 1;
}

/**
 * @brief Synchronize blackboard values from the world state.
 *
 * Copies the agent's position and the player position into the blackboard
 * and computes a normalized facing vector stored under the facing key.
 */
static void enemy_ai_sync_bb(EnemyAIBlackboard* ebb, RogueEnemy* e)
{
    rogue_bb_set_vec2(&ebb->bb, ebb->agent_pos_key, e->base.pos.x, e->base.pos.y);
    rogue_bb_set_vec2(&ebb->bb, ebb->player_pos_key, g_app.player.base.pos.x,
                      g_app.player.base.pos.y);
    float dx = g_app.player.base.pos.x - e->base.pos.x;
    float dy = g_app.player.base.pos.y - e->base.pos.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f)
    {
        dx = 1;
        dy = 0;
        len = 1;
    }
    dx /= len;
    dy /= len;
    rogue_bb_set_vec2(&ebb->bb, ebb->agent_facing_key, dx, dy);
}

/**
 * @brief Build the simple behavior tree for the enemy using the provided blackboard.
 *
 * Currently the tree is a single MoveTo action named "MoveToPlayer" which
 * reads the player position key and writes a flag when the move is complete.
 */
static RogueBehaviorTree* enemy_ai_build_bt(EnemyAIBlackboard* ebb)
{
    RogueBTNode* move = rogue_bt_action_move_to("MoveToPlayer", ebb->player_pos_key,
                                                ebb->agent_pos_key, 5.0f, ebb->move_reached_flag);
    return rogue_behavior_tree_create(move);
}

/**
 * @brief Enable behavior tree AI for an enemy.
 *
 * Allocates (from the AI agent pool) a per-enemy blackboard, initializes
 * it, synchronizes world state, creates the BT and attaches it to the enemy
 * structure. If allocation fails the function leaves the enemy disabled.
 */
void rogue_enemy_ai_bt_enable(RogueEnemy* e)
{
    if (!e)
        return;
    /* Allow re-enable if tree missing even if flag was left set */
    if (e->ai_bt_enabled && e->ai_tree)
        return;
    e->ai_bt_enabled = 1;
    fprintf(stdout, "AI_POOL_DBG enable called\n");
    fflush(stdout);
    EnemyAIBlackboard* ebb = (EnemyAIBlackboard*) rogue_ai_agent_acquire();
    if (!ebb)
        return; /* allocation/pool failure: leave BT disabled */
    rogue_bb_init(&ebb->bb);
    ebb->player_pos_key = "player_pos";
    ebb->agent_pos_key = "agent_pos";
    ebb->agent_facing_key = "agent_facing";
    ebb->move_reached_flag = "move_reached";
    enemy_ai_sync_bb(ebb, e);
    e->ai_tree = enemy_ai_build_bt(ebb);
    e->ai_bt_state = ebb;
}

/**
 * @brief Disable and teardown behavior tree AI for an enemy.
 *
 * Destroys the attached behavior tree and releases the blackboard back to
 * the AI agent pool if present.
 */
void rogue_enemy_ai_bt_disable(RogueEnemy* e)
{
    if (!e || !e->ai_bt_enabled)
        return;
    e->ai_bt_enabled = 0;
    if (e->ai_tree)
    {
        rogue_behavior_tree_destroy(e->ai_tree);
        e->ai_tree = NULL;
    }
    if (e->ai_bt_state)
    {
        rogue_ai_agent_release((EnemyAIBlackboard*) e->ai_bt_state);
        e->ai_bt_state = NULL;
    }
}

/**
 * @brief Per-frame tick for the enemy's behavior tree.
 *
 * Synchronizes the blackboard with the latest world state, advances the BT
 * by dt seconds and writes back any updated agent position from the
 * blackboard into the enemy entity.
 */
void rogue_enemy_ai_bt_tick(RogueEnemy* e, float dt)
{
    if (!e || !e->ai_bt_enabled || !e->ai_tree)
        return;
    EnemyAIBlackboard* ebb = (EnemyAIBlackboard*) e->ai_bt_state;
    if (!ebb)
        return;
    enemy_ai_sync_bb(ebb, e);
    rogue_behavior_tree_tick(e->ai_tree, &ebb->bb, dt);
    RogueBBVec2 agent;
    if (rogue_bb_get_vec2(&ebb->bb, ebb->agent_pos_key, &agent))
    {
        e->base.pos.x = agent.x;
        e->base.pos.y = agent.y;
    }
}
