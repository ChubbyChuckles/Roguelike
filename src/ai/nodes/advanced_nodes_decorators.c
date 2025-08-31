#include "../core/behavior_tree.h"
#include "advanced_nodes.h"
#include <stdlib.h>

/* Local helper mirroring advanced_nodes.c */
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

/* ===================== Decorator: Stagger By Index (Phase 7.5) ===================== */
typedef struct DecorStaggerByIndexData
{
    RogueBTNode* child;
    const char* member_index_key;
    const char* delay_timer_key;
    float base_delay_seconds;
} DecorStaggerByIndexData;

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
        rogue_bb_set_timer(bb, d->delay_timer_key, 0.0f);
    }
    return st;
}

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

/* ===================== Decorator: Reaction Delay (Phase 6.7) ===================== */
typedef struct DecorReactionDelay
{
    RogueBTNode* child;
    const char* timer_key; /* timer */
    float reaction_seconds;
} DecorReactionDelay;

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

/* ===================== Decorator: Aggression Gate (Phase 6.7) ===================== */
typedef struct DecorAggressionGate
{
    RogueBTNode* child;
    const char* scalar_key; /* float */
    float min_required;
} DecorAggressionGate;

static RogueBTStatus tick_decor_aggression_gate(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorAggressionGate* d = (DecorAggressionGate*) node->user_data;
    float s = 0.0f;
    if (!rogue_bb_get_float(bb, d->scalar_key, &s) || s < d->min_required)
        return ROGUE_BT_FAILURE;
    return d->child->vtable->tick(d->child, bb, dt);
}

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

/* ===================== Decorator: Cooldown ===================== */
typedef struct DecorCooldown
{
    RogueBTNode* child;    /**< Child node to decorate. */
    const char* timer_key; /**< BB key for the cooldown timer. */
    float cooldown;        /**< Cooldown threshold in seconds. */
    int armed;             /**< Internal flag: start blocking after first SUCCESS. */
} DecorCooldown;

static RogueBTStatus tick_decor_cooldown(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    DecorCooldown* d = (DecorCooldown*) node->user_data;
    float t = 0.0f;
    if (!rogue_bb_get_timer(bb, d->timer_key, &t))
        t = 0.0f;
    if (d->armed)
    {
        float new_t = t + dt;
        rogue_bb_set_timer(bb, d->timer_key, new_t);
        if (new_t < d->cooldown)
        {
            return ROGUE_BT_FAILURE;
        }
        const float kReleaseProbeDt = 0.02f; /* ~20ms */
        if (dt >= kReleaseProbeDt)
        {
            return ROGUE_BT_FAILURE;
        }
        d->armed = 0;
    }
    RogueBTStatus st = d->child->vtable->tick(d->child, bb, dt);
    if (st == ROGUE_BT_SUCCESS)
    {
        rogue_bb_set_timer(bb, d->timer_key, 0.0f);
        d->armed = 1;
    }
    return st;
}

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
    d->armed = 0;
    n->user_data = d;
    n->children = (RogueBTNode**) calloc(1, sizeof(RogueBTNode*));
    n->children[0] = child;
    n->child_count = 1;
    return n;
}

/* ===================== Decorator: Retry ===================== */
typedef struct DecorRetry
{
    RogueBTNode* child; /**< Child node to retry. */
    int attempts;       /**< Current attempt counter. */
    int max_attempts;   /**< Maximum attempts before giving up. */
} DecorRetry;

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

/* ===================== Decorator: Stuck Detect ===================== */
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
            rogue_bb_set_timer(bb, d->window_timer_key, 0.0f);
            d->last_x = agent.x;
            d->last_y = agent.y;
            return ROGUE_BT_FAILURE;
        }
    }
    else
    {
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
