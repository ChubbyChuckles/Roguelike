#ifndef ROGUE_AI_BASIC_NODES_H
#define ROGUE_AI_BASIC_NODES_H

#include "../core/behavior_tree.h"
#include "../core/blackboard.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Composite nodes
    RogueBTNode* rogue_bt_selector(const char* name); // runs children until one succeeds
    RogueBTNode* rogue_bt_sequence(const char* name); // runs children until one fails

    // Leaf utility nodes (stubs for now)
    RogueBTNode* rogue_bt_leaf_always_success(const char* name);
    RogueBTNode* rogue_bt_leaf_always_failure(const char* name);

    // Example conditional: checks bool blackboard key
    RogueBTNode* rogue_bt_leaf_check_bool(const char* name, const char* bb_key, bool expected);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_AI_BASIC_NODES_H
