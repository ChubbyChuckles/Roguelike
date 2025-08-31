#include "../core/behavior_tree.h"
#include "advanced_nodes.h"
#include <stdlib.h>

/**
 * Control nodes and shared cleanup for advanced AI nodes.
 * - Parallel aggregator
 * - Utility Selector (with per-child scorers)
 * - rogue_bt_advanced_cleanup hook used by behavior_tree.c
 */

/* ===================== Parallel ===================== */

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

RogueBTNode* rogue_bt_parallel(const char* name)
{
    return rogue_bt_node_create(name, 2, tick_parallel);
}

/* ===================== Utility Selector ===================== */

typedef struct UtilityChildMeta
{
    RogueUtilityScorer scorer; /* scorer callback and user data */
} UtilityChildMeta;

typedef struct UtilitySelectorData
{
    UtilityChildMeta* metas; /* sized to node->child_capacity */
} UtilitySelectorData;

static void util_selector_dtor(void* p)
{
    UtilitySelectorData* d = (UtilitySelectorData*) p;
    if (!d)
        return;
    if (d->metas)
        free(d->metas);
    free(d);
}

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

RogueBTNode* rogue_bt_utility_selector(const char* name)
{
    RogueBTNode* n = rogue_bt_node_create(name, 2, tick_utility_selector);
    if (n)
    {
        UtilitySelectorData* d = (UtilitySelectorData*) calloc(1, sizeof(UtilitySelectorData));
        n->user_data = d;
        n->user_data_dtor = util_selector_dtor; /* ensure metas and data are freed */
    }
    return n;
}

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

/* ===================== Advanced cleanup hook ===================== */

void rogue_bt_advanced_cleanup(RogueBTNode* node)
{
    if (!node || !node->user_data)
        return;
    /* Utility selector stored nested metas; user_data_dtor handles freeing, so nothing else. */
    if (node->vtable && node->vtable->tick == tick_utility_selector)
    {
        /* No-op: util_selector_dtor already frees metas + data via user_data_dtor. */
        return;
    }
    /* Other advanced nodes: cleanup handled by user_data_dtor when provided. */
}
