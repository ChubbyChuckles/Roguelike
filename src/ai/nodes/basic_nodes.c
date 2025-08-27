#include "basic_nodes.h"
#include "../core/behavior_tree.h"
#include <stdlib.h>

/**
 * @file basic_nodes.c
 * @brief Basic behavior tree node implementations.
 *
 * This file provides fundamental behavior tree nodes including composite nodes
 * (Selector, Sequence) and leaf nodes (Success, Failure, Boolean Check).
 * These form the foundation for building more complex AI behaviors.
 */

/** @brief Internal tick functions */

/**
 * @brief Tick function for Selector composite node.
 *
 * A Selector executes its children in order until one succeeds or is running.
 * Returns SUCCESS on first child success, RUNNING on first child running,
 * FAILURE only if all children fail.
 *
 * @param node The selector node being ticked.
 * @param bb The blackboard providing shared data (unused by selector).
 * @param dt Delta time since last tick (unused by selector).
 * @return RogueBTStatus SUCCESS if any child succeeds, RUNNING if any child is running, FAILURE
 * otherwise.
 */
static RogueBTStatus tick_selector(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        RogueBTNode* c = node->children[i];
        RogueBTStatus st = c->vtable->tick(c, bb, dt);
        rogue_bt_mark_node(c, st);
        if (st == ROGUE_BT_SUCCESS)
        {
            rogue_bt_mark_node(node, ROGUE_BT_SUCCESS);
            return ROGUE_BT_SUCCESS;
        }
        if (st == ROGUE_BT_RUNNING)
        {
            rogue_bt_mark_node(node, ROGUE_BT_RUNNING);
            return ROGUE_BT_RUNNING;
        }
    }
    rogue_bt_mark_node(node, ROGUE_BT_FAILURE);
    return ROGUE_BT_FAILURE;
}

/**
 * @brief Tick function for Sequence composite node.
 *
 * A Sequence executes its children in order until one fails or is running.
 * Returns FAILURE on first child failure, RUNNING on first child running,
 * SUCCESS only if all children succeed.
 *
 * @param node The sequence node being ticked.
 * @param bb The blackboard providing shared data (unused by sequence).
 * @param dt Delta time since last tick (unused by sequence).
 * @return RogueBTStatus FAILURE if any child fails, RUNNING if any child is running, SUCCESS
 * otherwise.
 */
static RogueBTStatus tick_sequence(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) bb;
    (void) dt;
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
        {
            rogue_bt_mark_node(node, ROGUE_BT_RUNNING);
            return ROGUE_BT_RUNNING;
        }
    }
    rogue_bt_mark_node(node, ROGUE_BT_SUCCESS);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Tick function for Always Success leaf node.
 *
 * This node always returns SUCCESS regardless of conditions. Useful for
 * fallback behaviors or testing.
 *
 * @param node The success node being ticked.
 * @param bb The blackboard (unused by this node).
 * @param dt Delta time (unused by this node).
 * @return RogueBTStatus Always returns SUCCESS.
 */
static RogueBTStatus tick_success(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) node;
    (void) bb;
    (void) dt;
    if (node)
        rogue_bt_mark_node(node, ROGUE_BT_SUCCESS);
    return ROGUE_BT_SUCCESS;
}

/**
 * @brief Tick function for Always Failure leaf node.
 *
 * This node always returns FAILURE regardless of conditions. Useful for
 * disabling behaviors or testing.
 *
 * @param node The failure node being ticked.
 * @param bb The blackboard (unused by this node).
 * @param dt Delta time (unused by this node).
 * @return RogueBTStatus Always returns FAILURE.
 */
static RogueBTStatus tick_failure(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) node;
    (void) bb;
    (void) dt;
    if (node)
        rogue_bt_mark_node(node, ROGUE_BT_FAILURE);
    return ROGUE_BT_FAILURE;
}

/**
 * @brief Data structure for boolean check leaf node.
 *
 * Stores the blackboard key to check and the expected boolean value.
 */
typedef struct CheckBoolData
{
    const char* key; /**< Blackboard key containing the boolean value to check. */
    bool expected;   /**< Expected boolean value for success. */
} CheckBoolData;

/**
 * @brief Tick function for Boolean Check leaf node.
 *
 * Checks if a boolean value in the blackboard matches the expected value.
 * Returns SUCCESS if the value matches, FAILURE otherwise or if the key doesn't exist.
 *
 * @param node The boolean check node being ticked.
 * @param bb The blackboard containing the boolean value.
 * @param dt Delta time (unused by this node).
 * @return RogueBTStatus SUCCESS if boolean matches expected, FAILURE otherwise.
 */
static RogueBTStatus tick_check_bool(RogueBTNode* node, RogueBlackboard* bb, float dt)
{
    (void) dt;
    if (!node || !bb)
        return ROGUE_BT_FAILURE;
    CheckBoolData* data = (CheckBoolData*) node->user_data;
    bool val = false;
    if (!rogue_bb_get_bool(bb, data->key, &val))
    {
        if (node)
            rogue_bt_mark_node(node, ROGUE_BT_FAILURE);
        return ROGUE_BT_FAILURE;
    }
    RogueBTStatus st = (val == data->expected) ? ROGUE_BT_SUCCESS : ROGUE_BT_FAILURE;
    rogue_bt_mark_node(node, st);
    return st;
}

/**
 * @brief Create a Selector composite node.
 *
 * A Selector executes children in order until one succeeds. This is useful for
 * implementing fallback behaviors (try option A, if that fails try option B, etc.).
 *
 * @param name Human-readable name for the node (may be NULL).
 * @return RogueBTNode* Newly allocated selector node, or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_selector(const char* name)
{
    return rogue_bt_node_create(name, 2, tick_selector);
}

/**
 * @brief Create a Sequence composite node.
 *
 * A Sequence executes children in order until one fails. This is useful for
 * implementing prerequisite behaviors (do A, then B, then C, all must succeed).
 *
 * @param name Human-readable name for the node (may be NULL).
 * @return RogueBTNode* Newly allocated sequence node, or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_sequence(const char* name)
{
    return rogue_bt_node_create(name, 2, tick_sequence);
}

/**
 * @brief Create an Always Success leaf node.
 *
 * This node always returns SUCCESS when ticked, regardless of conditions.
 * Useful for fallback behaviors, debugging, or ensuring certain branches always execute.
 *
 * @param name Human-readable name for the node (may be NULL).
 * @return RogueBTNode* Newly allocated success node, or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_leaf_always_success(const char* name)
{
    return rogue_bt_node_create(name, 0, tick_success);
}

/**
 * @brief Create an Always Failure leaf node.
 *
 * This node always returns FAILURE when ticked, regardless of conditions.
 * Useful for disabling behaviors, debugging, or creating conditional branches.
 *
 * @param name Human-readable name for the node (may be NULL).
 * @return RogueBTNode* Newly allocated failure node, or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_leaf_always_failure(const char* name)
{
    return rogue_bt_node_create(name, 0, tick_failure);
}

/**
 * @brief Create a Boolean Check leaf node.
 *
 * This node checks if a boolean value in the blackboard matches an expected value.
 * Returns SUCCESS if the value matches the expected value, FAILURE otherwise.
 *
 * @param name Human-readable name for the node (may be NULL).
 * @param bb_key Blackboard key containing the boolean value to check.
 * @param expected Expected boolean value for the node to return SUCCESS.
 * @return RogueBTNode* Newly allocated boolean check node, or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_leaf_check_bool(const char* name, const char* bb_key, bool expected)
{
    RogueBTNode* n = rogue_bt_node_create(name, 0, tick_check_bool);
    if (!n)
        return NULL;
    CheckBoolData* data = (CheckBoolData*) calloc(1, sizeof(CheckBoolData));
    data->key = bb_key;
    data->expected = expected;
    n->user_data = data;
    n->user_data_dtor = free;
    return n;
}
