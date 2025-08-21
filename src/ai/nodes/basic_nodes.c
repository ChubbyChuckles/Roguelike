#include "basic_nodes.h"
#include <stdlib.h>

// Internal tick functions
static RogueBTStatus tick_selector(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        RogueBTNode* c = node->children[i];
        RogueBTStatus st = c->vtable->tick(c, bb, dt);
        if (st == ROGUE_BT_SUCCESS)
            return ROGUE_BT_SUCCESS;
        if (st == ROGUE_BT_RUNNING)
            return ROGUE_BT_RUNNING;
    }
    return ROGUE_BT_FAILURE;
}

static RogueBTStatus tick_sequence(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        RogueBTNode* c = node->children[i];
        RogueBTStatus st = c->vtable->tick(c, bb, dt);
        if (st == ROGUE_BT_FAILURE)
            return ROGUE_BT_FAILURE;
        if (st == ROGUE_BT_RUNNING)
            return ROGUE_BT_RUNNING;
    }
    return ROGUE_BT_SUCCESS;
}

static RogueBTStatus tick_success(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) node;
    (void) bb;
    (void) dt;
    return ROGUE_BT_SUCCESS;
}
static RogueBTStatus tick_failure(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) node;
    (void) bb;
    (void) dt;
    return ROGUE_BT_FAILURE;
}

typedef struct CheckBoolData
{
    const char* key;
    bool expected;
} CheckBoolData;
static RogueBTStatus tick_check_bool(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    if (!node || !bb)
        return ROGUE_BT_FAILURE;
    CheckBoolData* data = (CheckBoolData*) node->user_data;
    bool val = false;
    if (!rogue_bb_get_bool(bb, data->key, &val))
        return ROGUE_BT_FAILURE;
    return (val == data->expected) ? ROGUE_BT_SUCCESS : ROGUE_BT_FAILURE;
}

RogueBTNode* rogue_bt_selector(const char* name)
{
    return rogue_bt_node_create(name, 2, tick_selector);
}
RogueBTNode* rogue_bt_sequence(const char* name)
{
    return rogue_bt_node_create(name, 2, tick_sequence);
}
RogueBTNode* rogue_bt_leaf_always_success(const char* name)
{
    return rogue_bt_node_create(name, 0, tick_success);
}
RogueBTNode* rogue_bt_leaf_always_failure(const char* name)
{
    return rogue_bt_node_create(name, 0, tick_failure);
}

RogueBTNode* rogue_bt_leaf_check_bool(const char* name, const char* bb_key, bool expected)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_check_bool);
    if (!n)
        return NULL;
    CheckBoolData* data = (CheckBoolData*) calloc(1, sizeof(CheckBoolData));
    data->key = bb_key;
    data->expected = expected;
    n->user_data = data;
    return n;
}
