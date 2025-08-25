#include "advanced_nodes.h"
#include "../../core/projectiles/projectiles.h"
#include "../core/behavior_tree.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file advanced_nodes.c
 * @brief Advanced behavior-tree node implementations and helpers.
 *
 * This file provides higher-level behavior tree nodes (parallel, utility selector,
 * various conditions, tactical actions, and decorators) used by the AI subsystem.
 * Existing inline comments are preserved; all public factory functions and
 * internal helpers are documented with Doxygen blocks to ease API generation.
 */

/**
 * @brief Ensure a node has an allocated children array.
 *
 * Allocates the children pointer on the provided node if it is NULL. The
 * function will initialize a default capacity of 4 if the node's
 * child_capacity is zero. This is a best-effort helper and returns false on
 * allocation failure or invalid input.
 *
 * @param n Pointer to the behavior tree node to ensure children for.
 * @return int 1 if children array exists or was successfully allocated,
 *             0 on failure or if n is NULL.
 */
static int ensure_child_array(RogueBTNode* n)
{
    if (!n)
        return 0;
    if (n->children)
        return 1;
    if (n->child_capacity == 0)
        n->child_capacity = 4;
    n->children = (RogueBTNode**) calloc(n->child_capacity, sizeof(RogueBTNode*));
    return n->children != NULL;
}

/**
 * @brief Tick function for a Parallel node.
 *
 * The Parallel node ticks all children. If any child fails the whole node
 * fails immediately. If any child is still running the Parallel node returns
 * ROGUE_BT_RUNNING. Only when all children succeed will the Parallel node
 * return ROGUE_BT_SUCCESS.
 *
 * @param node The parallel node being ticked.
 * @param bb The blackboard providing shared data for children.
 * @param dt Delta time since last tick (seconds).
 * @return RogueBTStatus The resulting status (SUCCESS, RUNNING, or FAILURE).
 */
static RogueBTStatus tick_parallel(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    int any_running = 0;
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        RogueBTNode* c = node->children[i];
        RogueBTStatus st = c->vtable->tick(c, bb, dt);
        rogue_bt_mark_node(c, st);
        if (st == ROGUE_BT_FAILURE)
        {
            rogue_bt_mark_node(node, ROGUE_BT_FAILURE);
            return ROGUE_BT_FAILURE;
        }
        if (st == ROGUE_BT_RUNNING)
            any_running = 1;
    }
    rogue_bt_mark_node(node, any_running ? ROGUE_BT_RUNNING : ROGUE_BT_SUCCESS);
    return node->last_status;
}
/**
 * @brief Create a Parallel behavior-tree node.
 *
 * A Parallel node will tick all its children each frame and aggregate their
 * results as described in @ref tick_parallel.
 *
 * @param name Human-readable name for the node (may be NULL).
 * @return RogueBTNode* Allocated node on success, or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_parallel(const char* name)
{
    return rogue_bt_node_create(name, 2, tick_parallel);
}

/**
 * @brief Per-child metadata for utility selector nodes.
 *
 * Stores the scorer callback and its user data for a single child of a utility
 * selector node.
 */
typedef struct UtilityChildMeta
{
    RogueUtilityScorer scorer; /**< Scorer used to evaluate this child. */
} UtilityChildMeta;

/**
 * @brief Runtime data for a utility selector node.
 *
 * Holds an array of UtilityChildMeta entries, allocated to match the
 * node->child_capacity. The array is resized when children are added.
 */
typedef struct UtilitySelectorData
{
    UtilityChildMeta* metas; /**< Array of per-child scorer metadata. */
} UtilitySelectorData;
/**
 * @brief Tick function for the Utility Selector node.
 *
 * Evaluates all child scorer functions and executes the child with the
 * highest score. If no scorer is available the default score is 0.0. Returns
 * FAILURE when the node or its data are invalid or when there are no
 * children. The chosen child's tick result is propagated as this node's
 * result.
 *
 * @param node The utility selector node being ticked.
 * @param bb The shared blackboard.
 * @param dt Delta time (unused by scorers but passed through to children).
 * @return RogueBTStatus Result of the chosen child's tick or FAILURE on error.
 */
static RogueBTStatus tick_utility_selector(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    if (!node)
        return ROGUE_BT_FAILURE;
    UtilitySelectorData* d = (UtilitySelectorData*) node->user_data;
    if (!d)
        return ROGUE_BT_FAILURE;
    if (node->child_count == 0)
        return ROGUE_BT_FAILURE;
    float best = -1e30f;
    int best_i = -1;
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        float s = 0.0f;
        if (d->metas && d->metas[i].scorer.fn)
            s = d->metas[i].scorer.fn(bb, d->metas[i].scorer.user_data);
        if (s > best)
        {
            best = s;
            best_i = i;
        }
    }
    if (best_i < 0)
        return ROGUE_BT_FAILURE;
    RogueBTStatus st = node->children[best_i]->vtable->tick(node->children[best_i], bb, dt);
    rogue_bt_mark_node(node->children[best_i], st);
    rogue_bt_mark_node(node, st);
    return st;
}
/**
 * @brief Create a Utility Selector node.
 *
 * The utility selector maintains per-child scorers that return a float score
 * indicating the desirability of executing that child. The child with the
 * highest score is ticked when this node runs.
 *
 * @param name Optional name for the node.
 * @return RogueBTNode* Newly allocated utility selector node, or NULL on
 *                      allocation failure.
 */
RogueBTNode* rogue_bt_utility_selector(const char* name)
{
    RogueBTNode* n = rogue_bt_node_create(name, 2, tick_utility_selector);
    if (n)
    {
        UtilitySelectorData* d = (UtilitySelectorData*) calloc(1, sizeof(UtilitySelectorData));
        n->user_data = d;
        n->user_data_dtor = free; /* free UtilitySelectorData in destroy; metas freed in cleanup */
    }
    return n;
}
/**
 * @brief Add a child to a utility selector and assign a scorer.
 *
 * This helper adds the provided child to the utility selector's child list
 * and stores the scorer callback in the per-child metadata array. The
 * function resizes the metadata array to match the node's child capacity when
 * needed.
 *
 * @param utility_node Utility selector node previously created by
 *                     rogue_bt_utility_selector.
 * @param child Child node to add.
 * @param scorer Scorer callback and user_data to evaluate the child.
 * @return int 1 on success, 0 on failure (invalid args, wrong node type,
 *             allocation failure).
 */
int rogue_bt_utility_set_child_scorer(RogueBTNode* utility_node, RogueBTNode* child,
                                      RogueUtilityScorer scorer)
{
    if (!utility_node || !child)
        return 0;
    if (utility_node->vtable->tick != tick_utility_selector)
        return 0;
    UtilitySelectorData* d = (UtilitySelectorData*) utility_node->user_data;
    if (!d)
        return 0;
    if (!rogue_bt_node_add_child(utility_node, child))
        return 0;
    /* Resize metas to match child_capacity */
    if (!d->metas)
    {
        d->metas =
            (UtilityChildMeta*) calloc(utility_node->child_capacity, sizeof(UtilityChildMeta));
    }
    else if (utility_node->child_capacity > 0)
    {
        d->metas = (UtilityChildMeta*) realloc(d->metas, utility_node->child_capacity *
                                                             sizeof(UtilityChildMeta));
    }
    d->metas[utility_node->child_count - 1].scorer = scorer;
    return 1;
}

/**
 * @brief Custom cleanup hook for advanced nodes' user_data.
 *
 * Frees node->user_data for nodes that allocated runtime data. The utility
 * selector stores an array of metas and requires a dedicated cleanup path; all
 * other advanced nodes are assumed to have a single allocation and are freed
 * directly.
 *
 * @param node Node whose user_data should be freed. No-op if node or
 *             node->user_data is NULL.
 */
void rogue_bt_advanced_cleanup(RogueBTNode* node)
{
    if (!node)
        return;
    if (!node->user_data)
        return;
    if (node->vtable && node->vtable->tick == tick_utility_selector)
    {
        UtilitySelectorData* d = (UtilitySelectorData*) node->user_data;
        if (d)
        {
            if (d->metas)
                free(d->metas);
        }
        return;
    }
    /* Other advanced nodes: main allocation is owned by user_data_dtor when set. */
}

/* =====================
   Phase 7: Group Tactics & Coordination
   ===================== */

/**
 * @brief Data for Phase 7.1 squad metadata assignment.
 *
 * Holds blackboard key names and literal values for writing squad_id,
 * member_index, and member_total for the current agent.
 */
typedef struct SquadSetIdsData
{
    const char* squad_id_key;
    int squad_id;
    const char* member_index_key;
    int member_index;
    const char* member_total_key;
    int member_total;
} SquadSetIdsData;

/**
 * @brief Tick for squad metadata assignment.
 *
 * Writes configured squad_id, member_index, and member_total into the
 * blackboard. Always returns SUCCESS.
 *
 * @param node Node with SquadSetIdsData.
 * @param bb Blackboard to write values into.
 * @param dt Unused.
 */
static RogueBTStatus tick_squad_set_ids(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    SquadSetIdsData* d = (SquadSetIdsData*) node->user_data;
    rogue_bb_set_int(bb, d->squad_id_key, d->squad_id);
    rogue_bb_set_int(bb, d->member_index_key, d->member_index);
    rogue_bb_set_int(bb, d->member_total_key, d->member_total);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for the squad metadata assignment node (Phase 7.1).
 *
 * Creates a node that writes squad_id, member_index, and member_total to
 * the blackboard using provided keys.
 *
 * @param name Optional node name.
 * @param bb_squad_id_key Blackboard key to write squad id.
 * @param squad_id Squad id value.
 * @param bb_member_index_key Blackboard key to write member index.
 * @param member_index Member index value.
 * @param bb_member_total_key Blackboard key to write member total.
 * @param member_total Member total value.
 * @return RogueBTNode* Allocated node or NULL on failure.
 */
RogueBTNode* rogue_bt_tactical_squad_set_ids(const char* name, const char* bb_squad_id_key,
                                             int squad_id, const char* bb_member_index_key,
                                             int member_index, const char* bb_member_total_key,
                                             int member_total)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_squad_set_ids);
    if (!n)
        return NULL;
    SquadSetIdsData* d = (SquadSetIdsData*) calloc(1, sizeof(SquadSetIdsData));
    d->squad_id_key = bb_squad_id_key;
    d->squad_id = squad_id;
    d->member_index_key = bb_member_index_key;
    d->member_index = member_index;
    d->member_total_key = bb_member_total_key;
    d->member_total = member_total;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for Phase 7.2 role assignment.
 *
 * Contains key names for member index/total and optional weights for roles
 * Bruiser/Harrier/Support, plus an output role key (int 0/1/2).
 */
typedef struct RoleAssignData
{
    const char* out_role_key;
    const char* member_index_key;
    const char* member_total_key;
    const char* w_bruiser_key;
    const char* w_harrier_key;
    const char* w_support_key;
} RoleAssignData;

/**
 * @brief Tick for role assignment (Phase 7.2).
 *
 * If any weight keys are present, selects the role with the largest weight.
 * Otherwise assigns role = member_index % 3. Writes result to out_role_key.
 */
static RogueBTStatus tick_role_assign(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    RoleAssignData* d = (RoleAssignData*) node->user_data;
    int idx = 0, total = 0;
    rogue_bb_get_int(bb, d->member_index_key, &idx);
    rogue_bb_get_int(bb, d->member_total_key, &total);
    float wb = 0, wh = 0, ws = 0;
    bool hb = rogue_bb_get_float(bb, d->w_bruiser_key, &wb);
    bool hh = rogue_bb_get_float(bb, d->w_harrier_key, &wh);
    bool hs = rogue_bb_get_float(bb, d->w_support_key, &ws);
    int role = 0; /* 0=Bruiser,1=Harrier,2=Support */
    if (hb || hh || hs)
    {
        float best = wb;
        role = 0;
        if (wh > best)
        {
            best = wh;
            role = 1;
        }
        if (ws > best)
        {
            role = 2;
        }
    }
    else
    {
        if (total <= 0)
            total = 3;
        role = idx % 3;
    }
    rogue_bb_set_int(bb, d->out_role_key, role);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for role assignment node (Phase 7.2).
 *
 * Optional weight keys bias role choice; absent weights fall back to idx%3.
 */
RogueBTNode* rogue_bt_tactical_role_assign(const char* name, const char* bb_out_role_key,
                                           const char* bb_member_index_key,
                                           const char* bb_member_total_key,
                                           const char* bb_weight_bruiser_key,
                                           const char* bb_weight_harrier_key,
                                           const char* bb_weight_support_key)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_role_assign);
    if (!n)
        return NULL;
    RoleAssignData* d = (RoleAssignData*) calloc(1, sizeof(RoleAssignData));
    d->out_role_key = bb_out_role_key;
    d->member_index_key = bb_member_index_key;
    d->member_total_key = bb_member_total_key;
    d->w_bruiser_key = bb_weight_bruiser_key;
    d->w_harrier_key = bb_weight_harrier_key;
    d->w_support_key = bb_weight_support_key;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for Phase 7.3 surround/encircle slot assignment.
 *
 * Keys for target position and member index/total, desired circle radius,
 * and output point key to store the computed slot.
 */
typedef struct SurroundAssignData
{
    const char* target_pos_key;
    const char* member_index_key;
    const char* member_total_key;
    float radius;
    const char* out_point_key;
} SurroundAssignData;

/**
 * @brief Tick for surround slot assignment (Phase 7.3).
 *
 * Places an evenly spaced point on a circle around the target based on
 * member_index/member_total and writes it to out_point_key.
 */
static RogueBTStatus tick_surround_assign(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    SurroundAssignData* d = (SurroundAssignData*) node->user_data;
    RogueBBVec2 target;
    if (!rogue_bb_get_vec2(bb, d->target_pos_key, &target))
        return ROGUE_BT_FAILURE;
    int idx = 0, total = 0;
    rogue_bb_get_int(bb, d->member_index_key, &idx);
    rogue_bb_get_int(bb, d->member_total_key, &total);
    if (total <= 0)
        total = 1;
    float t = (float) idx / (float) total;
    /* Use a local PI constant to avoid relying on non-standard M_PI macro */
    const float PI_F = 3.14159265358979323846f;
    float angle = t * 2.0f * PI_F;
    float x = target.x + cosf(angle) * d->radius;
    float y = target.y + sinf(angle) * d->radius;
    rogue_bb_set_vec2(bb, d->out_point_key, x, y);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for surround slot assignment node (Phase 7.3).
 */
RogueBTNode* rogue_bt_tactical_surround_assign_slot(const char* name, const char* bb_target_pos_key,
                                                    const char* bb_member_index_key,
                                                    const char* bb_member_total_key, float radius,
                                                    const char* bb_out_point_key)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_surround_assign);
    if (!n)
        return NULL;
    SurroundAssignData* d = (SurroundAssignData*) calloc(1, sizeof(SurroundAssignData));
    d->target_pos_key = bb_target_pos_key;
    d->member_index_key = bb_member_index_key;
    d->member_total_key = bb_member_total_key;
    d->radius = radius;
    d->out_point_key = bb_out_point_key;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for Phase 7.4 retreat condition.
 *
 * Succeeds if HP% < min_pct or recent_deaths >= deaths_threshold.
 */
typedef struct CondShouldRetreatData
{
    const char* self_hp_pct_key;
    float min_pct;
    const char* recent_deaths_key;
    int deaths_threshold;
} CondShouldRetreatData;

/**
 * @brief Tick for retreat condition (Phase 7.4).
 */
static RogueBTStatus tick_cond_should_retreat(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    CondShouldRetreatData* d = (CondShouldRetreatData*) node->user_data;
    float hp = 1.0f;
    int deaths = 0;
    bool ok_hp = rogue_bb_get_float(bb, d->self_hp_pct_key, &hp);
    rogue_bb_get_int(bb, d->recent_deaths_key, &deaths);
    if (ok_hp && hp < d->min_pct)
        return ROGUE_BT_SUCCESS;
    if (deaths >= d->deaths_threshold)
        return ROGUE_BT_SUCCESS;
    return ROGUE_BT_FAILURE;
}

/**
 * @brief Factory for retreat condition node (Phase 7.4).
 */
RogueBTNode* rogue_bt_condition_should_retreat(const char* name, const char* bb_self_hp_pct_key,
                                               float min_pct, const char* bb_recent_deaths_key,
                                               int deaths_threshold)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_cond_should_retreat);
    if (!n)
        return NULL;
    CondShouldRetreatData* d = (CondShouldRetreatData*) calloc(1, sizeof(CondShouldRetreatData));
    d->self_hp_pct_key = bb_self_hp_pct_key;
    d->min_pct = min_pct;
    d->recent_deaths_key = bb_recent_deaths_key;
    d->deaths_threshold = deaths_threshold;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for Phase 7.5 stagger-by-index decorator.
 *
 * Delays child execution by base_delay_seconds * member_index using a
 * named timer.
 */
typedef struct DecorStaggerByIndexData
{
    RogueBTNode* child;
    const char* member_index_key;
    const char* delay_timer_key;
    float base_delay_seconds;
} DecorStaggerByIndexData;

/**
 * @brief Tick for stagger-by-index decorator (Phase 7.5).
 *
 * Accumulates dt into delay_timer_key and gates child until the computed
 * delay has elapsed. Resets the timer on child SUCCESS.
 */
static RogueBTStatus tick_decor_stagger_by_index(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorStaggerByIndexData* d = (DecorStaggerByIndexData*) node->user_data;
    int idx = 0;
    rogue_bb_get_int(bb, d->member_index_key, &idx);
    float t = 0.0f;
    rogue_bb_get_timer(bb, d->delay_timer_key, &t);
    t += dt;
    rogue_bb_set_timer(bb, d->delay_timer_key, t);
    float needed = d->base_delay_seconds * (float) (idx < 0 ? 0 : idx);
    if (t < needed)
        return ROGUE_BT_RUNNING;
    RogueBTStatus st = d->child->vtable->tick(d->child, bb, dt);
    if (st == ROGUE_BT_SUCCESS)
    {
        /* reset for next chain round */
        rogue_bb_set_timer(bb, d->delay_timer_key, 0.0f);
    }
    return st;
}

/**
 * @brief Factory for stagger-by-index decorator node (Phase 7.5).
 */
RogueBTNode* rogue_bt_decorator_stagger_by_index(const char* name, RogueBTNode* child,
                                                 const char* bb_member_index_key,
                                                 const char* bb_delay_timer_key,
                                                 float base_delay_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 1, tick_decor_stagger_by_index);
    if (!n)
        return NULL;
    DecorStaggerByIndexData* d =
        (DecorStaggerByIndexData*) calloc(1, sizeof(DecorStaggerByIndexData));
    d->child = child;
    d->member_index_key = bb_member_index_key;
    d->delay_timer_key = bb_delay_timer_key;
    d->base_delay_seconds = base_delay_seconds;
    n->user_data = d;
    ensure_child_array(n);
    rogue_bt_node_add_child(n, child);
    return n;
}

/**
 * @brief Condition data for checking if the player is visible to an agent.
 *
 * The condition reads player and agent positions and the agent facing
 * vector from the blackboard and queries the perception helper.
 */
typedef struct CondPlayerVisible
{
    const char* player_pos_key;   /**< BB key for player position (vec2). */
    const char* agent_pos_key;    /**< BB key for agent position (vec2). */
    const char* agent_facing_key; /**< BB key for agent facing vector (vec2). */
    float fov_deg;                /**< Field-of-view angle in degrees. */
    float max_dist;               /**< Maximum visible distance. */
} CondPlayerVisible;
/**
 * @brief Tick for the PlayerVisible condition node.
 *
 * Reads positions and facing from the blackboard and delegates to the
 * perception subsystem (rogue_perception_can_see). Returns SUCCESS if the
 * player is visible within the provided FOV and distance, otherwise FAILURE.
 *
 * @param node Condition node with CondPlayerVisible in node->user_data.
 * @param bb Shared blackboard.
 * @param dt Unused.
 * @return RogueBTStatus SUCCESS if visible, FAILURE otherwise.
 */
static RogueBTStatus tick_cond_player_visible(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    CondPlayerVisible* d = (CondPlayerVisible*) node->user_data;
    RogueBBVec2 player, agent, facing;
    if (!rogue_bb_get_vec2(bb, d->player_pos_key, &player))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_facing_key, &facing))
        return ROGUE_BT_FAILURE;
    RoguePerceptionAgent pa = {0};
    pa.x = agent.x;
    pa.y = agent.y;
    pa.facing_x = facing.x;
    pa.facing_y = facing.y;
    return rogue_perception_can_see(&pa, player.x, player.y, d->fov_deg, d->max_dist, NULL)
               ? ROGUE_BT_SUCCESS
               : ROGUE_BT_FAILURE;
}
/**
 * @brief Factory for a PlayerVisible condition node.
 *
 * @param name Node name.
 * @param bb_player_pos_key Blackboard key for player position (vec2).
 * @param bb_agent_pos_key Blackboard key for agent position (vec2).
 * @param bb_agent_facing_key Blackboard key for agent facing vector (vec2).
 * @param fov_deg Field of view angle in degrees.
 * @param max_dist Maximum visible distance.
 * @return RogueBTNode* Allocated condition node or NULL on failure.
 */
RogueBTNode* rogue_bt_condition_player_visible(const char* name, const char* bb_player_pos_key,
                                               const char* bb_agent_pos_key,
                                               const char* bb_agent_facing_key, float fov_deg,
                                               float max_dist)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_cond_player_visible);
    if (!n)
        return NULL;
    CondPlayerVisible* d = (CondPlayerVisible*) calloc(1, sizeof(CondPlayerVisible));
    d->player_pos_key = bb_player_pos_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->agent_facing_key = bb_agent_facing_key;
    d->fov_deg = fov_deg;
    d->max_dist = max_dist;
    n->user_data = d;
    return n;
}

/**
 * @brief Condition that succeeds when a named timer has reached a minimum.
 */
typedef struct CondTimerElapsed
{
    const char* timer_key; /**< BB key for the timer value. */
    float min_value;       /**< Minimum timer value to consider elapsed. */
} CondTimerElapsed;
/**
 * @brief Tick for the TimerElapsed condition node.
 *
 * Reads a timer from the blackboard and returns SUCCESS when its value is
 * greater than or equal to min_value.
 */
static RogueBTStatus tick_cond_timer_elapsed(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    CondTimerElapsed* d = (CondTimerElapsed*) node->user_data;
    float t = 0.0f;
    if (!rogue_bb_get_timer(bb, d->timer_key, &t))
        return ROGUE_BT_FAILURE;
    return (t >= d->min_value) ? ROGUE_BT_SUCCESS : ROGUE_BT_FAILURE;
}
/**
 * @brief Factory for a TimerElapsed condition node.
 *
 * @param name Node name.
 * @param bb_timer_key Blackboard key containing the timer value.
 * @param min_value Threshold for success.
 * @return RogueBTNode* Allocated node or NULL on failure.
 */
RogueBTNode* rogue_bt_condition_timer_elapsed(const char* name, const char* bb_timer_key,
                                              float min_value)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_cond_timer_elapsed);
    if (!n)
        return NULL;
    CondTimerElapsed* d = (CondTimerElapsed*) calloc(1, sizeof(CondTimerElapsed));
    d->timer_key = bb_timer_key;
    d->min_value = min_value;
    n->user_data = d;
    return n;
}

/**
 * @brief Condition that checks if a health value is below a threshold.
 */
typedef struct CondHealthBelow
{
    const char* health_key; /**< BB key for health (float). */
    float threshold;        /**< Threshold below which the condition succeeds. */
} CondHealthBelow;
/**
 * @brief Tick for HealthBelow condition node.
 *
 * Returns SUCCESS when the float at health_key is less than threshold.
 */
static RogueBTStatus tick_cond_health_below(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    CondHealthBelow* d = (CondHealthBelow*) node->user_data;
    float hp = 0.0f;
    if (!rogue_bb_get_float(bb, d->health_key, &hp))
        return ROGUE_BT_FAILURE;
    return (hp < d->threshold) ? ROGUE_BT_SUCCESS : ROGUE_BT_FAILURE;
}
/**
 * @brief Factory for a HealthBelow condition node.
 *
 * @param name Node name.
 * @param bb_health_key Blackboard key storing the float health value.
 * @param threshold Threshold to compare against.
 * @return RogueBTNode* Allocated node or NULL on failure.
 */
RogueBTNode* rogue_bt_condition_health_below(const char* name, const char* bb_health_key,
                                             float threshold)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_cond_health_below);
    if (!n)
        return NULL;
    CondHealthBelow* d = (CondHealthBelow*) calloc(1, sizeof(CondHealthBelow));
    d->health_key = bb_health_key;
    d->threshold = threshold;
    n->user_data = d;
    return n;
}

/**
 * @brief Action data for moving an agent toward a target position.
 *
 * The action reads a target vec2 and the agent vec2, moves the agent toward
 * the target at the specified speed, writes the updated agent vec2 back to
 * the blackboard, and sets a reached flag when close enough.
 */
typedef struct ActionMoveTo
{
    const char* target_pos_key;   /**< BB key for target position (vec2). */
    const char* agent_pos_key;    /**< BB key for agent position (vec2). */
    float speed;                  /**< Movement speed (units per second). */
    const char* reached_flag_key; /**< BB key for boolean 'reached' flag. */
} ActionMoveTo;
/**
 * @brief Tick function for the MoveTo action.
 *
 * Moves the agent toward the target position, sets the reached flag to true
 * when within a small threshold, otherwise returns RUNNING while moving.
 */
static RogueBTStatus tick_action_move_to(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    ActionMoveTo* d = (ActionMoveTo*) node->user_data;
    RogueBBVec2 target, agent;
    if (!rogue_bb_get_vec2(bb, d->target_pos_key, &target))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    float dx = target.x - agent.x, dy = target.y - agent.y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 < 0.05f)
    {
        rogue_bb_set_bool(bb, d->reached_flag_key, true);
        return ROGUE_BT_SUCCESS;
    }
    float dist = sqrtf(dist2);
    float nx = dx / dist, ny = dy / dist;
    agent.x += nx * d->speed * dt;
    agent.y += ny * d->speed * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, agent.x, agent.y);
    rogue_bb_set_bool(bb, d->reached_flag_key, false);
    return ROGUE_BT_RUNNING;
}
/**
 * @brief Factory for the MoveTo action node.
 *
 * @param name Node name.
 * @param bb_target_pos_key Blackboard key for the target position.
 * @param bb_agent_pos_key Blackboard key for the agent position.
 * @param speed Movement speed.
 * @param bb_out_reached_flag Blackboard key for the reached boolean flag.
 * @return RogueBTNode* Allocated action node or NULL on failure.
 */
RogueBTNode* rogue_bt_action_move_to(const char* name, const char* bb_target_pos_key,
                                     const char* bb_agent_pos_key, float speed,
                                     const char* bb_out_reached_flag)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_move_to);
    if (!n)
        return NULL;
    ActionMoveTo* d = (ActionMoveTo*) calloc(1, sizeof(ActionMoveTo));
    d->target_pos_key = bb_target_pos_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->speed = speed;
    d->reached_flag_key = bb_out_reached_flag;
    n->user_data = d;
    return n;
}

/**
 * @brief Action data for fleeing away from a threat position.
 */
typedef struct ActionFleeFrom
{
    const char* threat_pos_key; /**< BB key for threat position (vec2). */
    const char* agent_pos_key;  /**< BB key for agent position (vec2). */
    float speed;                /**< Fleeing speed (units per second). */
} ActionFleeFrom;
/**
 * @brief Tick implementation for the FleeFrom action.
 *
 * Moves the agent directly away from the threat position at the configured
 * speed. Always returns RUNNING unless the blackboard keys are missing.
 */
static RogueBTStatus tick_action_flee_from(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    ActionFleeFrom* d = (ActionFleeFrom*) node->user_data;
    RogueBBVec2 threat, agent;
    if (!rogue_bb_get_vec2(bb, d->threat_pos_key, &threat))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    float dx = agent.x - threat.x, dy = agent.y - threat.y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 < 0.0001f)
    {
        dx = 1;
        dy = 0;
        dist2 = 1;
    }
    float dist = sqrtf(dist2);
    agent.x += (dx / dist) * d->speed * dt;
    agent.y += (dy / dist) * d->speed * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, agent.x, agent.y);
    return ROGUE_BT_RUNNING;
}
/**
 * @brief Factory for the FleeFrom action node.
 *
 * @param name Node name.
 * @param bb_threat_pos_key Blackboard key for the threat position.
 * @param bb_agent_pos_key Blackboard key for the agent position.
 * @param speed Speed to flee at.
 * @return RogueBTNode* Allocated node or NULL on failure.
 */
RogueBTNode* rogue_bt_action_flee_from(const char* name, const char* bb_threat_pos_key,
                                       const char* bb_agent_pos_key, float speed)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_flee_from);
    if (!n)
        return NULL;
    ActionFleeFrom* d = (ActionFleeFrom*) calloc(1, sizeof(ActionFleeFrom));
    d->threat_pos_key = bb_threat_pos_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->speed = speed;
    n->user_data = d;
    return n;
}

/**
 * @brief Action data for melee or ranged attack checks.
 *
 * The action checks a boolean flag (in-range or clear-line) and resets the
 * configured cooldown timer when an attack is initiated. The cooldown value is
 * stored for informational purposes and reset behavior in the tick.
 */
typedef struct ActionAttack
{
    const char* flag_key;           /**< BB key for the in-range/clear-line flag. */
    const char* cooldown_timer_key; /**< BB key for the cooldown timer (float). */
    float cooldown;                 /**< Configured cooldown in seconds (informational). */
} ActionAttack;
/**
 * @brief Tick for melee attack action.
 *
 * Succeeds immediately when the in-range flag is true and resets the
 * cooldown timer. Otherwise returns FAILURE.
 */
static RogueBTStatus tick_action_attack_melee(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    ActionAttack* d = (ActionAttack*) node->user_data;
    bool in_range = false;
    if (!rogue_bb_get_bool(bb, d->flag_key, &in_range) || !in_range)
        return ROGUE_BT_FAILURE; /* begin attack: reset cooldown timer */
    rogue_bb_set_timer(bb, d->cooldown_timer_key, 0.0f);
    return ROGUE_BT_SUCCESS;
}
/**
 * @brief Factory for melee attack action node.
 */
RogueBTNode* rogue_bt_action_attack_melee(const char* name, const char* bb_in_range_flag_key,
                                          const char* bb_cooldown_timer_key, float cooldown_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_attack_melee);
    if (!n)
        return NULL;
    ActionAttack* d = (ActionAttack*) calloc(1, sizeof(ActionAttack));
    d->flag_key = bb_in_range_flag_key;
    d->cooldown_timer_key = bb_cooldown_timer_key;
    d->cooldown = cooldown_seconds;
    n->user_data = d;
    return n;
}
/**
 * @brief Tick for ranged attack action.
 *
 * Similar to melee attack but checks a line-clear flag before succeeding and
 * resetting the cooldown timer.
 */
static RogueBTStatus tick_action_attack_ranged(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    ActionAttack* d = (ActionAttack*) node->user_data;
    bool clear = false;
    if (!rogue_bb_get_bool(bb, d->flag_key, &clear) || !clear)
        return ROGUE_BT_FAILURE;
    rogue_bb_set_timer(bb, d->cooldown_timer_key, 0.0f);
    return ROGUE_BT_SUCCESS;
}
/**
 * @brief Factory for ranged attack action node.
 */
RogueBTNode* rogue_bt_action_attack_ranged(const char* name, const char* bb_line_clear_flag_key,
                                           const char* bb_cooldown_timer_key,
                                           float cooldown_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_attack_ranged);
    if (!n)
        return NULL;
    ActionAttack* d = (ActionAttack*) calloc(1, sizeof(ActionAttack));
    d->flag_key = bb_line_clear_flag_key;
    d->cooldown_timer_key = bb_cooldown_timer_key;
    d->cooldown = cooldown_seconds;
    n->user_data = d;
    return n;
}

/**
 * @brief Action to strafe perpendicular to the vector from agent to target.
 *
 * Alternates left/right based on a boolean flag stored in the blackboard and
 * runs for a fixed duration, after which it flips the flag and returns
 * SUCCESS.
 */
typedef struct ActionStrafe
{
    const char* target_pos_key; /**< BB key for target position. */
    const char* agent_pos_key;  /**< BB key for agent position. */
    const char* left_flag_key;  /**< BB key for the left/right toggle flag. */
    float speed;                /**< Strafing speed. */
    float duration;             /**< Total duration to strafe. */
    float elapsed;              /**< Elapsed time since start. */
    int direction;              /**< Current direction multiplier (-1 or 1). */
} ActionStrafe;
/**
 * @brief Tick for the Strafe action.
 *
 * Computes a perpendicular vector to the target direction and moves the
 * agent along that perpendicular for the configured duration.
 */
static RogueBTStatus tick_action_strafe(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    ActionStrafe* d = (ActionStrafe*) node->user_data;
    RogueBBVec2 target, agent;
    if (!rogue_bb_get_vec2(bb, d->target_pos_key, &target))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    bool left = false;
    rogue_bb_get_bool(bb, d->left_flag_key, &left);
    d->direction = left ? -1 : 1;
    float vx = target.x - agent.x, vy = target.y - agent.y;
    float len = sqrtf(vx * vx + vy * vy);
    if (len < 0.0001f)
    {
        vx = 1;
        vy = 0;
        len = 1;
    }
    vx /= len;
    vy /= len; /* perpendicular */
    float px = -vy * d->direction;
    float py = vx * d->direction;
    agent.x += px * d->speed * dt;
    agent.y += py * d->speed * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, agent.x, agent.y);
    d->elapsed += dt;
    if (d->elapsed >= d->duration)
    { /* flip flag for next time */
        rogue_bb_set_bool(bb, d->left_flag_key, !left);
        return ROGUE_BT_SUCCESS;
    }
    return ROGUE_BT_RUNNING;
}
/**
 * @brief Factory for the Strafe action node.
 */
RogueBTNode* rogue_bt_action_strafe(const char* name, const char* bb_target_pos_key,
                                    const char* bb_agent_pos_key,
                                    const char* bb_strafe_left_flag_key, float speed,
                                    float duration_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_strafe);
    if (!n)
        return NULL;
    ActionStrafe* d = (ActionStrafe*) calloc(1, sizeof(ActionStrafe));
    d->target_pos_key = bb_target_pos_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->left_flag_key = bb_strafe_left_flag_key;
    d->speed = speed;
    d->duration = duration_seconds;
    d->elapsed = 0.0f;
    d->direction = 1;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for Phase 6.1 ranged projectile firing action.
 *
 * Fires a projectile from agent toward target when optional line-clear flag is true.
 * On fire, resets optional cooldown timer to 0. Returns SUCCESS on fire, FAILURE otherwise.
 */
typedef struct ActionRangedFire
{
    const char* agent_pos_key;
    const char* target_pos_key;
    const char* opt_line_flag_key;  /* may be NULL -> ignore */
    const char* opt_cool_timer_key; /* may be NULL -> ignore */
    float speed;
    float life_ms;
    int damage;
} ActionRangedFire;

/**
 * @brief Tick for ranged projectile firing.
 *
 * Validates optional line-of-fire flag, computes direction from agent to target,
 * spawns a projectile with a slight forward offset, and optionally resets a cooldown timer.
 *
 * @param node Node with ActionRangedFire in user_data.
 * @param bb Blackboard with agent/target positions and optional flags/timer.
 * @param dt Unused.
 * @return RogueBTStatus SUCCESS on projectile spawn, FAILURE otherwise.
 */
static RogueBTStatus tick_action_ranged_fire(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    ActionRangedFire* d = (ActionRangedFire*) node->user_data;
    if (!d)
        return ROGUE_BT_FAILURE;
    /* Optional gate */
    if (d->opt_line_flag_key)
    {
        bool ok = false;
        if (!rogue_bb_get_bool(bb, d->opt_line_flag_key, &ok) || !ok)
            return ROGUE_BT_FAILURE;
    }
    RogueBBVec2 agent, target;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->target_pos_key, &target))
        return ROGUE_BT_FAILURE;
    float dx = target.x - agent.x;
    float dy = target.y - agent.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f)
    {
        dx = 1.0f;
        dy = 0.0f;
        len = 1.0f;
    }
    dx /= len;
    dy /= len;
    /* Small forward offset to avoid immediate overlap */
    float sx = agent.x + dx * 0.5f;
    float sy = agent.y + dy * 0.5f;
    rogue_projectiles_spawn(sx, sy, dx, dy, d->speed, d->life_ms, d->damage);
    if (d->opt_cool_timer_key)
        rogue_bb_set_timer(bb, d->opt_cool_timer_key, 0.0f);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for ranged projectile firing action (Phase 6.1).
 *
 * @param name Node name.
 * @param bb_agent_pos_key Blackboard key for agent position (vec2).
 * @param bb_target_pos_key Blackboard key for target position (vec2).
 * @param bb_optional_line_clear_flag_key Optional boolean gate for firing.
 * @param bb_optional_cooldown_timer_key Optional cooldown timer key to reset on fire.
 * @param speed_tiles_per_sec Projectile speed in tiles per second.
 * @param life_ms Projectile lifetime in milliseconds.
 * @param damage Damage dealt by the projectile.
 * @return RogueBTNode* Allocated node or NULL on failure.
 */
RogueBTNode* rogue_bt_action_ranged_fire_projectile(const char* name, const char* bb_agent_pos_key,
                                                    const char* bb_target_pos_key,
                                                    const char* bb_optional_line_clear_flag_key,
                                                    const char* bb_optional_cooldown_timer_key,
                                                    float speed_tiles_per_sec, float life_ms,
                                                    int damage)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_ranged_fire);
    if (!n)
        return NULL;
    ActionRangedFire* d = (ActionRangedFire*) calloc(1, sizeof(ActionRangedFire));
    d->agent_pos_key = bb_agent_pos_key;
    d->target_pos_key = bb_target_pos_key;
    d->opt_line_flag_key = bb_optional_line_clear_flag_key;
    d->opt_cool_timer_key = bb_optional_cooldown_timer_key;
    d->speed = speed_tiles_per_sec;
    d->life_ms = life_ms;
    d->damage = damage;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for a tactical flank attempt action.
 *
 * Computes a flank point perpendicular to the agent->player vector at the
 * configured offset and writes the flank target to the blackboard.
 */
typedef struct TacticalFlank
{
    const char* player_pos_key; /**< BB key for player position. */
    const char* agent_pos_key;  /**< BB key for agent position. */
    const char* out_flank_key;  /**< BB key to store computed flank target. */
    float offset;               /**< Distance offset from player for flank point. */
} TacticalFlank;
/**
 * @brief Tick for the tactical flank computation action.
 *
 * Always returns SUCCESS after computing and writing the flank point.
 */
static RogueBTStatus tick_tactical_flank(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    TacticalFlank* d = (TacticalFlank*) node->user_data;
    RogueBBVec2 player, agent;
    if (!rogue_bb_get_vec2(bb, d->player_pos_key, &player))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    float vx = player.x - agent.x, vy = player.y - agent.y;
    float len = sqrtf(vx * vx + vy * vy);
    if (len < 0.0001f)
    {
        vx = 1;
        vy = 0;
        len = 1;
    }
    vx /= len;
    vy /= len; /* choose perpendicular based on deterministic sign: prefer left (negative y) */
    float px = -vy;
    float py = vx;
    float flank_x = player.x + px * d->offset;
    float flank_y = player.y + py * d->offset;
    rogue_bb_set_vec2(bb, d->out_flank_key, flank_x, flank_y);
    return ROGUE_BT_SUCCESS;
}
/**
 * @brief Factory for the Tactical Flank Attempt node.
 */
RogueBTNode* rogue_bt_tactical_flank_attempt(const char* name, const char* bb_player_pos_key,
                                             const char* bb_agent_pos_key,
                                             const char* bb_out_flank_target_key, float offset)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_tactical_flank);
    if (!n)
        return NULL;
    TacticalFlank* d = (TacticalFlank*) calloc(1, sizeof(TacticalFlank));
    d->player_pos_key = bb_player_pos_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->out_flank_key = bb_out_flank_target_key;
    d->offset = offset;
    n->user_data = d;
    return n;
}

/* ===================== Phase 6.2: Reaction Windows (Parry / Dodge) ===================== */
typedef struct ReactParry
{
    const char* incoming_flag_key; /* bool */
    const char* parry_active_key;  /* bool out */
    const char* timer_key;         /* timer in bb */
    float window_seconds;
} ReactParry;

/**
 * @brief Tick for parry reaction window (Phase 6.2).
 *
 * When incoming threat flag is true, advances the parry timer; within the window,
 * sets parry_active to true and returns SUCCESS. Outside the window or without threat,
 * resets state and returns FAILURE.
 */
static RogueBTStatus tick_react_parry(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    ReactParry* d = (ReactParry*) node->user_data;
    bool incoming = false;
    (void) dt;
    if (!rogue_bb_get_bool(bb, d->incoming_flag_key, &incoming) || !incoming)
    {
        /* No threat: reset parry */
        rogue_bb_set_bool(bb, d->parry_active_key, false);
        rogue_bb_set_timer(bb, d->timer_key, 0.0f);
        return ROGUE_BT_FAILURE;
    }
    float t = 0.0f;
    rogue_bb_get_timer(bb, d->timer_key, &t);
    t += dt;
    rogue_bb_set_timer(bb, d->timer_key, t);
    if (t <= d->window_seconds)
    {
        rogue_bb_set_bool(bb, d->parry_active_key, true);
        return ROGUE_BT_SUCCESS;
    }
    /* Window elapsed */
    rogue_bb_set_bool(bb, d->parry_active_key, false);
    return ROGUE_BT_FAILURE;
}

/**
 * @brief Factory for parry reaction window action (Phase 6.2).
 */
RogueBTNode* rogue_bt_action_react_parry(const char* name, const char* bb_incoming_threat_flag_key,
                                         const char* bb_out_parry_active_key,
                                         const char* bb_parry_timer_key, float window_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_react_parry);
    if (!n)
        return NULL;
    ReactParry* d = (ReactParry*) calloc(1, sizeof(ReactParry));
    d->incoming_flag_key = bb_incoming_threat_flag_key;
    d->parry_active_key = bb_out_parry_active_key;
    d->timer_key = bb_parry_timer_key;
    d->window_seconds = window_seconds;
    n->user_data = d;
    return n;
}

typedef struct ReactDodge
{
    const char* incoming_flag_key; /* bool */
    const char* agent_pos_key;     /* vec2 */
    const char* threat_pos_key;    /* vec2 */
    const char* out_dodge_vec_key; /* vec2 */
    const char* timer_key;         /* timer */
    float duration_seconds;
} ReactDodge;

/**
 * @brief Tick for dodge reaction window (Phase 6.2).
 *
 * When threat is incoming, computes and outputs a normalized dodge vector away from threat,
 * advances a timer, and returns SUCCESS while within the duration. Otherwise returns FAILURE.
 */
static RogueBTStatus tick_react_dodge(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    ReactDodge* d = (ReactDodge*) node->user_data;
    bool incoming = false;
    (void) dt;
    if (!rogue_bb_get_bool(bb, d->incoming_flag_key, &incoming) || !incoming)
    {
        /* No threat: reset */
        rogue_bb_set_timer(bb, d->timer_key, 0.0f);
        return ROGUE_BT_FAILURE;
    }

    float t = 0.0f;
    rogue_bb_get_timer(bb, d->timer_key, &t);
    t += dt;
    rogue_bb_set_timer(bb, d->timer_key, t);

    /* Compute dodge vector away from threat on first activation or keep last */
    RogueBBVec2 agent, threat;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent) ||
        !rogue_bb_get_vec2(bb, d->threat_pos_key, &threat))
    {
        return ROGUE_BT_FAILURE;
    }
    float dx = agent.x - threat.x;
    float dy = agent.y - threat.y;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f)
    {
        dx = 1.0f;
        dy = 0.0f;
        len = 1.0f;
    }
    dx /= len;
    dy /= len;
    rogue_bb_set_vec2(bb, d->out_dodge_vec_key, dx, dy);

    if (t <= d->duration_seconds)
        return ROGUE_BT_SUCCESS;
    return ROGUE_BT_FAILURE;
}

/**
 * @brief Factory for dodge reaction window action (Phase 6.2).
 */
RogueBTNode* rogue_bt_action_react_dodge(const char* name, const char* bb_incoming_threat_flag_key,
                                         const char* bb_agent_pos_key,
                                         const char* bb_threat_pos_key,
                                         const char* bb_out_dodge_vec_key,
                                         const char* bb_dodge_timer_key, float duration_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_react_dodge);
    if (!n)
        return NULL;
    ReactDodge* d = (ReactDodge*) calloc(1, sizeof(ReactDodge));
    d->incoming_flag_key = bb_incoming_threat_flag_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->threat_pos_key = bb_threat_pos_key;
    d->out_dodge_vec_key = bb_out_dodge_vec_key;
    d->timer_key = bb_dodge_timer_key;
    d->duration_seconds = duration_seconds;
    n->user_data = d;
    return n;
}

/* ===================== Phase 6.3: Opportunistic Attack ===================== */
typedef struct OpportunisticAttack
{
    const char* recovery_flag_key;  /* bool: target is in recovery */
    const char* agent_pos_key;      /* vec2 */
    const char* target_pos_key;     /* vec2 */
    float max_distance;             /* <=0 means ignore distance */
    const char* opt_cool_timer_key; /* timer: reset to 0 on success if provided */
} OpportunisticAttack;

/**
 * @brief Tick for opportunistic attack (Phase 6.3).
 *
 * Succeeds when the target is in recovery and (optionally) within max_distance.
 * Resets optional cooldown timer on success.
 */
static RogueBTStatus tick_action_opportunistic_attack(RogueBTNode* node, RogueBlackboard* bb,
                                                      float dt)
{
    (void) dt;
    OpportunisticAttack* d = (OpportunisticAttack*) node->user_data;
    bool in_recovery = false;
    if (!rogue_bb_get_bool(bb, d->recovery_flag_key, &in_recovery) || !in_recovery)
        return ROGUE_BT_FAILURE;
    if (d->max_distance > 0.0f)
    {
        RogueBBVec2 agent, target;
        if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent) ||
            !rogue_bb_get_vec2(bb, d->target_pos_key, &target))
            return ROGUE_BT_FAILURE;
        float dx = target.x - agent.x, dy = target.y - agent.y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 > d->max_distance * d->max_distance)
            return ROGUE_BT_FAILURE;
    }
    if (d->opt_cool_timer_key)
        rogue_bb_set_timer(bb, d->opt_cool_timer_key, 0.0f);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for opportunistic attack action (Phase 6.3).
 */
RogueBTNode* rogue_bt_action_opportunistic_attack(const char* name,
                                                  const char* bb_target_in_recovery_flag_key,
                                                  const char* bb_agent_pos_key,
                                                  const char* bb_target_pos_key,
                                                  float max_distance_allowed,
                                                  const char* bb_optional_cooldown_timer_key)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_opportunistic_attack);
    if (!n)
        return NULL;
    OpportunisticAttack* d = (OpportunisticAttack*) calloc(1, sizeof(OpportunisticAttack));
    d->recovery_flag_key = bb_target_in_recovery_flag_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->target_pos_key = bb_target_pos_key;
    d->max_distance = max_distance_allowed;
    d->opt_cool_timer_key = bb_optional_cooldown_timer_key;
    n->user_data = d;
    return n;
}

/* ===================== Phase 6.4: Kiting Logic (Preferred Distance Band) ===================== */
typedef struct ActionKiteBand
{
    const char* agent_pos_key;  /* vec2 */
    const char* target_pos_key; /* vec2 */
    float min_dist;             /* prefer >= min_dist */
    float max_dist;             /* prefer <= max_dist (if <= min, treated as =min) */
    float speed;                /* movement speed */
} ActionKiteBand;

/**
 * @brief Tick for kiting within a preferred distance band (Phase 6.4).
 *
 * Returns SUCCESS when within [min_dist, max_dist], otherwise moves agent either toward
 * or away to reach the band and returns RUNNING.
 */
static RogueBTStatus tick_action_kite_band(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    ActionKiteBand* d = (ActionKiteBand*) node->user_data;
    RogueBBVec2 agent, target;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->target_pos_key, &target))
        return ROGUE_BT_FAILURE;
    float dx = target.x - agent.x, dy = target.y - agent.y;
    float dist2 = dx * dx + dy * dy;
    float min_d = d->min_dist;
    float max_d = (d->max_dist <= d->min_dist) ? d->min_dist : d->max_dist;
    float min2 = min_d * min_d;
    float max2 = max_d * max_d;
    if (dist2 >= min2 && dist2 <= max2)
        return ROGUE_BT_SUCCESS; /* already in band */
    float len = sqrtf(dist2);
    if (len < 0.0001f)
    {
        dx = 1.0f;
        dy = 0.0f;
        len = 1.0f;
    }
    /* if too close -> move away, if too far -> move toward */
    float dirx = (dist2 < min2) ? -(dx / len) : (dx / len);
    float diry = (dist2 < min2) ? -(dy / len) : (dy / len);
    agent.x += dirx * d->speed * dt;
    agent.y += diry * d->speed * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, agent.x, agent.y);
    return ROGUE_BT_RUNNING;
}

/**
 * @brief Factory for kiting action within a preferred distance band (Phase 6.4).
 */
RogueBTNode* rogue_bt_action_kite_band(const char* name, const char* bb_agent_pos_key,
                                       const char* bb_target_pos_key, float preferred_min_distance,
                                       float preferred_max_distance, float move_speed)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_kite_band);
    if (!n)
        return NULL;
    ActionKiteBand* d = (ActionKiteBand*) calloc(1, sizeof(ActionKiteBand));
    d->agent_pos_key = bb_agent_pos_key;
    d->target_pos_key = bb_target_pos_key;
    d->min_dist = (preferred_min_distance < 0.0f) ? 0.0f : preferred_min_distance;
    d->max_dist = preferred_max_distance;
    d->speed = move_speed;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for a Tactical Regroup action.
 *
 * Moves the agent toward a regroup point until within a small radius, then
 * returns SUCCESS.
 */
typedef struct TacticalRegroup
{
    const char* regroup_pos_key; /**< BB key for regroup target position. */
    const char* agent_pos_key;   /**< BB key for agent position. */
    float speed;                 /**< Movement speed toward regroup point. */
} TacticalRegroup;
/**
 * @brief Tick for Tactical Regroup action.
 */
static RogueBTStatus tick_tactical_regroup(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    TacticalRegroup* d = (TacticalRegroup*) node->user_data;
    RogueBBVec2 target, agent;
    if (!rogue_bb_get_vec2(bb, d->regroup_pos_key, &target))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    float dx = target.x - agent.x, dy = target.y - agent.y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 < 0.04f)
    {
        return ROGUE_BT_SUCCESS;
    }
    float dist = sqrtf(dist2);
    agent.x += (dx / dist) * d->speed * dt;
    agent.y += (dy / dist) * d->speed * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, agent.x, agent.y);
    return ROGUE_BT_RUNNING;
}
/**
 * @brief Factory for the Tactical Regroup node.
 */
RogueBTNode* rogue_bt_tactical_regroup(const char* name, const char* bb_regroup_point_key,
                                       const char* bb_agent_pos_key, float speed)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_tactical_regroup);
    if (!n)
        return NULL;
    TacticalRegroup* d = (TacticalRegroup*) calloc(1, sizeof(TacticalRegroup));
    d->regroup_pos_key = bb_regroup_point_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->speed = speed;
    n->user_data = d;
    return n;
}

/**
 * @brief Data for seeking cover behind an obstacle.
 *
 * Computes a cover point on the perimeter of an obstacle opposite the player
 * and moves the agent to that point. When arrival and occlusion checks pass
 * the node returns SUCCESS and sets the out flag.
 */
typedef struct TacticalCoverSeek
{
    const char* player_pos_key;      /**< BB key for player position. */
    const char* agent_pos_key;       /**< BB key for agent position. */
    const char* obstacle_pos_key;    /**< BB key for obstacle center position. */
    const char* out_cover_point_key; /**< BB key to store computed cover point. */
    const char* out_flag_key;        /**< BB key to set when in cover. */
    float obstacle_radius;           /**< Radius of obstacle used for perimeter calc. */
    float speed;                     /**< Movement speed toward cover. */
    int computed;                    /**< Internal flag whether cover point computed. */
    float cover_x, cover_y;          /**< Cached cover point coordinates. */
} TacticalCoverSeek;
/**
 * @brief Tick for Tactical Cover Seek.
 *
 * Computes the cover point once and moves the agent toward it. Performs an
 * occlusion check on arrival and sets the out_flag to true when occluded.
 */
static RogueBTStatus tick_tactical_cover_seek(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    TacticalCoverSeek* d = (TacticalCoverSeek*) node->user_data;
    RogueBBVec2 player, agent, obstacle;
    if (!rogue_bb_get_vec2(bb, d->player_pos_key, &player))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    if (!rogue_bb_get_vec2(bb, d->obstacle_pos_key, &obstacle))
        return ROGUE_BT_FAILURE;
    if (!d->computed)
    {
        float vx = player.x - obstacle.x, vy = player.y - obstacle.y;
        float len = sqrtf(vx * vx + vy * vy);
        if (len < 0.0001f)
        {
            vx = 1;
            vy = 0;
            len = 1;
        }
        vx /= len;
        vy /= len; /* cover point is opposite side of obstacle from player */
        d->cover_x = obstacle.x - vx * d->obstacle_radius;
        d->cover_y = obstacle.y - vy * d->obstacle_radius;
        rogue_bb_set_vec2(bb, d->out_cover_point_key, d->cover_x, d->cover_y);
        d->computed = 1;
    }
    float dx = d->cover_x - agent.x, dy = d->cover_y - agent.y;
    float dist2 = dx * dx + dy * dy;
    if (dist2 < 0.04f)
    { /* arrival -> check occlusion: line from player to agent intersects obstacle circle? */
        float pvx = d->cover_x - player.x, pvy = d->cover_y - player.y;
        float ovx = obstacle.x - player.x,
              ovy = obstacle.y - player.y; /* Project obstacle center onto player->cover segment */
        float seg_len2 = pvx * pvx + pvy * pvy;
        if (seg_len2 > 0)
        {
            float t = ((ovx * pvx) + (ovy * pvy)) / seg_len2;
            if (t < 0)
                t = 0;
            else if (t > 1)
                t = 1;
            float projx = player.x + pvx * t;
            float projy = player.y + pvy * t;
            float cx = obstacle.x - projx, cy = obstacle.y - projy;
            float dist_c2 = cx * cx + cy * cy;
            if (dist_c2 <= d->obstacle_radius * d->obstacle_radius * 1.05f)
            {
                rogue_bb_set_bool(bb, d->out_flag_key, true);
                return ROGUE_BT_SUCCESS;
            }
        }
        /* If occlusion check fails treat as failure (no valid cover) */
        return ROGUE_BT_FAILURE;
    }
    float dist = sqrtf(dist2);
    agent.x += (dx / dist) * d->speed * dt;
    agent.y += (dy / dist) * d->speed * dt;
    rogue_bb_set_vec2(bb, d->agent_pos_key, agent.x, agent.y);
    return ROGUE_BT_RUNNING;
}
/**
 * @brief Factory for the Tactical Cover Seek node.
 */
RogueBTNode* rogue_bt_tactical_cover_seek(const char* name, const char* bb_player_pos_key,
                                          const char* bb_agent_pos_key,
                                          const char* bb_obstacle_pos_key,
                                          const char* bb_out_cover_point_key,
                                          const char* bb_out_in_cover_flag_key,
                                          float obstacle_radius, float move_speed)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_tactical_cover_seek);
    if (!n)
        return NULL;
    TacticalCoverSeek* d = (TacticalCoverSeek*) calloc(1, sizeof(TacticalCoverSeek));
    d->player_pos_key = bb_player_pos_key;
    d->agent_pos_key = bb_agent_pos_key;
    d->obstacle_pos_key = bb_obstacle_pos_key;
    d->out_cover_point_key = bb_out_cover_point_key;
    d->out_flag_key = bb_out_in_cover_flag_key;
    d->obstacle_radius = obstacle_radius;
    d->speed = move_speed;
    d->computed = 0;
    d->cover_x = 0;
    d->cover_y = 0;
    n->user_data = d;
    return n;
}

/* ===================== Phase 6.5: Focus Fire Coordination ===================== */
typedef struct FocusBroadcast
{
    const char* threat_score_key;      /* float */
    float leader_threshold;            /* minimum score to be leader */
    const char* target_pos_key;        /* vec2 */
    const char* out_group_focus_flag;  /* bool */
    const char* out_group_focus_pos;   /* vec2 */
    const char* group_focus_ttl_timer; /* timer */
} FocusBroadcast;

/**
 * @brief Tick for broadcasting a group focus if leader (Phase 6.5).
 *
 * When the agent's threat score is >= leader_threshold, writes group focus flag and position,
 * and resets the TTL timer. Otherwise returns FAILURE.
 */
static RogueBTStatus tick_focus_broadcast_if_leader(RogueBTNode* node, RogueBlackboard* bb,
                                                    float dt)
{
    (void) dt;
    FocusBroadcast* d = (FocusBroadcast*) node->user_data;
    float score = 0.0f;
    if (!rogue_bb_get_float(bb, d->threat_score_key, &score))
        return ROGUE_BT_FAILURE;
    if (score < d->leader_threshold)
        return ROGUE_BT_FAILURE;
    RogueBBVec2 target;
    if (!rogue_bb_get_vec2(bb, d->target_pos_key, &target))
        return ROGUE_BT_FAILURE;
    rogue_bb_set_bool(bb, d->out_group_focus_flag, true);
    rogue_bb_set_vec2(bb, d->out_group_focus_pos, target.x, target.y);
    rogue_bb_set_timer(bb, d->group_focus_ttl_timer, 0.0f);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for focus broadcast if leader (Phase 6.5).
 */
RogueBTNode* rogue_bt_tactical_focus_broadcast_if_leader(
    const char* name, const char* bb_threat_score_key, float leader_threshold,
    const char* bb_target_pos_key, const char* bb_out_group_focus_flag_key,
    const char* bb_out_group_focus_pos_key, const char* bb_group_focus_ttl_timer_key)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_focus_broadcast_if_leader);
    if (!n)
        return NULL;
    FocusBroadcast* d = (FocusBroadcast*) calloc(1, sizeof(FocusBroadcast));
    d->threat_score_key = bb_threat_score_key;
    d->leader_threshold = leader_threshold;
    d->target_pos_key = bb_target_pos_key;
    d->out_group_focus_flag = bb_out_group_focus_flag_key;
    d->out_group_focus_pos = bb_out_group_focus_pos_key;
    d->group_focus_ttl_timer = bb_group_focus_ttl_timer_key;
    n->user_data = d;
    return n;
}

typedef struct FocusDecay
{
    const char* flag_key;  /* bool */
    const char* timer_key; /* timer */
    float ttl_seconds;     /* time to keep focus active */
} FocusDecay;

/**
 * @brief Tick for group focus decay (Phase 6.5).
 *
 * Advances TTL timer when focus is active; disables flag and returns FAILURE once expired,
 * otherwise returns SUCCESS.
 */
static RogueBTStatus tick_focus_decay(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    FocusDecay* d = (FocusDecay*) node->user_data;
    bool active = false;
    rogue_bb_get_bool(bb, d->flag_key, &active);
    if (!active)
        return ROGUE_BT_FAILURE;
    float t = 0.0f;
    rogue_bb_get_timer(bb, d->timer_key, &t);
    t += dt;
    rogue_bb_set_timer(bb, d->timer_key, t);
    if (t >= d->ttl_seconds)
    {
        rogue_bb_set_bool(bb, d->flag_key, false);
        return ROGUE_BT_FAILURE;
    }
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for focus decay node (Phase 6.5).
 */
RogueBTNode* rogue_bt_tactical_focus_decay(const char* name, const char* bb_group_focus_flag_key,
                                           const char* bb_group_focus_ttl_timer_key,
                                           float ttl_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_focus_decay);
    if (!n)
        return NULL;
    FocusDecay* d = (FocusDecay*) calloc(1, sizeof(FocusDecay));
    d->flag_key = bb_group_focus_flag_key;
    d->timer_key = bb_group_focus_ttl_timer_key;
    d->ttl_seconds = ttl_seconds;
    n->user_data = d;
    return n;
}

/* ===================== Phase 6.6: Finisher Execute ===================== */
typedef struct ActionFinisher
{
    const char* target_health_key;  /* float */
    float threshold;                /* success when hp <= threshold */
    const char* agent_pos_key;      /* vec2 */
    const char* target_pos_key;     /* vec2 */
    float max_distance;             /* <=0 ignore */
    const char* opt_cool_timer_key; /* timer */
} ActionFinisher;

/**
 * @brief Tick for finisher execute (Phase 6.6).
 *
 * Succeeds when target health <= threshold and (optionally) within max distance; resets
 * optional cooldown timer on success.
 */
static RogueBTStatus tick_action_finisher_execute(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    ActionFinisher* d = (ActionFinisher*) node->user_data;
    float hp = 0.0f;
    if (!rogue_bb_get_float(bb, d->target_health_key, &hp))
        return ROGUE_BT_FAILURE;
    if (hp > d->threshold)
        return ROGUE_BT_FAILURE;
    if (d->max_distance > 0.0f)
    {
        RogueBBVec2 agent, target;
        if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent) ||
            !rogue_bb_get_vec2(bb, d->target_pos_key, &target))
            return ROGUE_BT_FAILURE;
        float dx = target.x - agent.x, dy = target.y - agent.y;
        float dist2 = dx * dx + dy * dy;
        if (dist2 > d->max_distance * d->max_distance)
            return ROGUE_BT_FAILURE;
    }
    if (d->opt_cool_timer_key)
        rogue_bb_set_timer(bb, d->opt_cool_timer_key, 0.0f);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Factory for finisher execute action (Phase 6.6).
 */
RogueBTNode* rogue_bt_action_finisher_execute(const char* name, const char* bb_target_health_key,
                                              float threshold, const char* bb_agent_pos_key,
                                              const char* bb_target_pos_key,
                                              float max_distance_allowed,
                                              const char* bb_optional_cooldown_timer_key)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_action_finisher_execute);
    if (!n)
        return NULL;
    ActionFinisher* d = (ActionFinisher*) calloc(1, sizeof(ActionFinisher));
    d->target_health_key = bb_target_health_key;
    d->threshold = threshold;
    d->agent_pos_key = bb_agent_pos_key;
    d->target_pos_key = bb_target_pos_key;
    d->max_distance = max_distance_allowed;
    d->opt_cool_timer_key = bb_optional_cooldown_timer_key;
    n->user_data = d;
    return n;
}

/* ===================== Phase 6.7: Difficulty Scaler Helpers ===================== */
typedef struct DecorReactionDelay
{
    RogueBTNode* child;
    const char* timer_key; /* timer */
    float reaction_seconds;
} DecorReactionDelay;

/**
 * @brief Tick for reaction delay decorator (Phase 6.7).
 *
 * Delays child execution until reaction_seconds have elapsed on the named timer.
 * Returns FAILURE while waiting; otherwise forwards child status.
 */
static RogueBTStatus tick_decor_reaction_delay(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorReactionDelay* d = (DecorReactionDelay*) node->user_data;
    float t = 0.0f;
    rogue_bb_get_timer(bb, d->timer_key, &t);
    if (t < d->reaction_seconds)
    {
        rogue_bb_set_timer(bb, d->timer_key, t + dt);
        return ROGUE_BT_FAILURE;
    }
    return d->child->vtable->tick(d->child, bb, dt);
}

/**
 * @brief Factory for reaction delay decorator (Phase 6.7).
 */
RogueBTNode* rogue_bt_decorator_reaction_delay(const char* name, RogueBTNode* child,
                                               const char* bb_reaction_timer_key,
                                               float reaction_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 1, tick_decor_reaction_delay);
    if (!n)
        return NULL;
    DecorReactionDelay* d = (DecorReactionDelay*) calloc(1, sizeof(DecorReactionDelay));
    d->child = child;
    d->timer_key = bb_reaction_timer_key;
    d->reaction_seconds = reaction_seconds;
    n->user_data = d;
    ensure_child_array(n);
    rogue_bt_node_add_child(n, child);
    return n;
}

typedef struct DecorAggressionGate
{
    RogueBTNode* child;
    const char* scalar_key; /* float */
    float min_required;
} DecorAggressionGate;

/**
 * @brief Tick for aggression gate decorator (Phase 6.7).
 *
 * Forwards child only when aggression scalar >= min_required; otherwise returns FAILURE.
 */
static RogueBTStatus tick_decor_aggression_gate(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorAggressionGate* d = (DecorAggressionGate*) node->user_data;
    float s = 0.0f;
    if (!rogue_bb_get_float(bb, d->scalar_key, &s) || s < d->min_required)
        return ROGUE_BT_FAILURE;
    return d->child->vtable->tick(d->child, bb, dt);
}

/**
 * @brief Factory for aggression gate decorator (Phase 6.7).
 */
RogueBTNode* rogue_bt_decorator_aggression_gate(const char* name, RogueBTNode* child,
                                                const char* bb_aggression_scalar_key,
                                                float min_required)
{
    RogueBTNode* n = rogue_bt_node_create(name, 1, tick_decor_aggression_gate);
    if (!n)
        return NULL;
    DecorAggressionGate* d = (DecorAggressionGate*) calloc(1, sizeof(DecorAggressionGate));
    d->child = child;
    d->scalar_key = bb_aggression_scalar_key;
    d->min_required = min_required;
    n->user_data = d;
    ensure_child_array(n);
    rogue_bt_node_add_child(n, child);
    return n;
}

/**
 * @brief Decorator that enforces a cooldown between child successes.
 *
 * The child is executed only when the named timer reaches the configured
 * cooldown threshold. When the child succeeds the timer is reset to 0.
 */
typedef struct DecorCooldown
{
    RogueBTNode* child;    /**< Child node to decorate. */
    const char* timer_key; /**< BB key for the cooldown timer. */
    float cooldown;        /**< Cooldown threshold in seconds. */
} DecorCooldown;
/**
 * @brief Tick for the cooldown decorator.
 *
 * Advances the timer while preventing child execution until the cooldown has
 * elapsed. Resets the timer on child success.
 */
static RogueBTStatus tick_decor_cooldown(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorCooldown* d = (DecorCooldown*) node->user_data;
    float t = 0.0f;
    if (!rogue_bb_get_timer(bb, d->timer_key, &t))
        return ROGUE_BT_FAILURE;
    /* Treat t==0 as "ready now" to allow the first execution immediately. */
    if (t < d->cooldown && t > 0.0f)
    {
        /* Cooldown in progress: accumulate elapsed and remain in FAILURE state. */
        rogue_bb_set_timer(bb, d->timer_key, t + dt);
        return ROGUE_BT_FAILURE;
    }
    RogueBTStatus st = d->child->vtable->tick(d->child, bb, dt);
    if (st == ROGUE_BT_SUCCESS)
    {
        rogue_bb_set_timer(bb, d->timer_key, 0.0f);
    }
    else
    { /* advance timer even if not success to avoid stall */
        rogue_bb_set_timer(bb, d->timer_key, t + dt);
    }
    return st;
}
/**
 * @brief Factory for the cooldown decorator node.
 */
RogueBTNode* rogue_bt_decorator_cooldown(const char* name, RogueBTNode* child,
                                         const char* bb_timer_key, float cooldown_seconds)
{
    RogueBTNode* n = rogue_bt_node_create(name, 1, tick_decor_cooldown);
    if (!n)
        return NULL;
    DecorCooldown* d = (DecorCooldown*) calloc(1, sizeof(DecorCooldown));
    d->child = child;
    d->timer_key = bb_timer_key;
    d->cooldown = cooldown_seconds;
    n->user_data = d;
    n->children = (RogueBTNode**) calloc(1, sizeof(RogueBTNode*));
    n->children[0] = child;
    n->child_count = 1;
    return n;
}

/**
 * @brief Decorator that retries its child up to max_attempts on failure.
 *
 * The attempts counter is incremented on FAILURE; while attempts <
 * max_attempts the decorator reports RUNNING to allow retry semantics. When
 * attempts reach max_attempts the decorator returns FAILURE.
 */
typedef struct DecorRetry
{
    RogueBTNode* child; /**< Child node to retry. */
    int attempts;       /**< Current attempt counter. */
    int max_attempts;   /**< Maximum attempts before giving up. */
} DecorRetry;
/**
 * @brief Tick for retry decorator.
 *
 * Executes the child and increments attempts on failure. Resets attempts on
 * non-failure statuses.
 */
static RogueBTStatus tick_decor_retry(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorRetry* d = (DecorRetry*) node->user_data;
    RogueBTStatus st = d->child->vtable->tick(d->child, bb, dt);
    if (st == ROGUE_BT_FAILURE)
    {
        d->attempts++;
        if (d->attempts < d->max_attempts)
            return ROGUE_BT_RUNNING;
        else
            return ROGUE_BT_FAILURE;
    }
    d->attempts = 0;
    return st;
}
/**
 * @brief Factory for the retry decorator node.
 *
 * @param name Node name.
 * @param child Child node to retry on failure.
 * @param max_attempts Maximum number of retry attempts allowed.
 * @return RogueBTNode* Allocated decorator node or NULL on failure.
 */
RogueBTNode* rogue_bt_decorator_retry(const char* name, RogueBTNode* child, int max_attempts)
{
    RogueBTNode* n = rogue_bt_node_create(name, 1, tick_decor_retry);
    if (!n)
        return NULL;
    DecorRetry* d = (DecorRetry*) calloc(1, sizeof(DecorRetry));
    d->child = child;
    d->attempts = 0;
    d->max_attempts = max_attempts;
    n->user_data = d;
    n->children = (RogueBTNode**) calloc(1, sizeof(RogueBTNode*));
    n->children[0] = child;
    n->child_count = 1;
    return n;
}

/**
 * @brief Decorator that detects if an agent is stuck (not moving enough for a time window).
 *
 * Tracks the last observed position and a progress timer stored in the blackboard. If the
 * displacement from last_pos is below min_move_threshold for at least window_seconds, returns
 * FAILURE and resets the timer. Otherwise ticks the child and forwards its status. The window
 * timer is advanced each tick when movement is below threshold.
 */
typedef struct DecorStuckDetect
{
    RogueBTNode* child;
    const char* agent_pos_key;
    const char* window_timer_key;
    float window_seconds;
    float min_move_threshold;
    int has_last;
    float last_x, last_y;
} DecorStuckDetect;

static RogueBTStatus tick_decor_stuck(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorStuckDetect* d = (DecorStuckDetect*) node->user_data;
    RogueBBVec2 agent;
    if (!rogue_bb_get_vec2(bb, d->agent_pos_key, &agent))
        return ROGUE_BT_FAILURE;
    if (!d->has_last)
    {
        d->last_x = agent.x;
        d->last_y = agent.y;
        d->has_last = 1;
        rogue_bb_set_timer(bb, d->window_timer_key, 0.0f);
    }
    float dx = agent.x - d->last_x;
    float dy = agent.y - d->last_y;
    float dist2 = dx * dx + dy * dy;
    float t = 0.0f;
    rogue_bb_get_timer(bb, d->window_timer_key, &t);
    if (dist2 < d->min_move_threshold * d->min_move_threshold)
    {
        t += dt;
        rogue_bb_set_timer(bb, d->window_timer_key, t);
        if (t >= d->window_seconds)
        {
            /* Declare stuck and reset window */
            rogue_bb_set_timer(bb, d->window_timer_key, 0.0f);
            d->last_x = agent.x;
            d->last_y = agent.y;
            return ROGUE_BT_FAILURE;
        }
    }
    else
    {
        /* Movement observed: reset window and update anchor */
        rogue_bb_set_timer(bb, d->window_timer_key, 0.0f);
        d->last_x = agent.x;
        d->last_y = agent.y;
    }
    return d->child->vtable->tick(d->child, bb, dt);
}

RogueBTNode* rogue_bt_decorator_stuck_detect(const char* name, RogueBTNode* child,
                                             const char* bb_agent_pos_key,
                                             const char* bb_window_timer_key, float window_seconds,
                                             float min_move_threshold)
{
    RogueBTNode* n = rogue_bt_node_create(name, 1, tick_decor_stuck);
    if (!n)
        return NULL;
    DecorStuckDetect* d = (DecorStuckDetect*) calloc(1, sizeof(DecorStuckDetect));
    d->child = child;
    d->agent_pos_key = bb_agent_pos_key;
    d->window_timer_key = bb_window_timer_key;
    d->window_seconds = window_seconds;
    d->min_move_threshold = min_move_threshold;
    d->has_last = 0;
    n->user_data = d;
    n->children = (RogueBTNode**) calloc(1, sizeof(RogueBTNode*));
    n->children[0] = child;
    n->child_count = 1;
    return n;
}
