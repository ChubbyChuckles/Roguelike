#ifndef ROGUE_AI_BEHAVIOR_TREE_H
#define ROGUE_AI_BEHAVIOR_TREE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" { 
#endif

// Execution result of a behavior tree node tick
typedef enum RogueBTStatus {
    ROGUE_BT_INVALID = 0,
    ROGUE_BT_SUCCESS,
    ROGUE_BT_FAILURE,
    ROGUE_BT_RUNNING
} RogueBTStatus;

// Forward declarations
struct RogueBTNode;
struct RogueBlackboard;

// Function pointer used to tick a node
typedef RogueBTStatus (*RogueBTNodeTick)(struct RogueBTNode* node, struct RogueBlackboard* blackboard, float dt);

typedef struct RogueBTNodeVTable {
    RogueBTNodeTick tick;
} RogueBTNodeVTable;

// Generic behavior tree node
typedef struct RogueBTNode {
    const RogueBTNodeVTable* vtable;
    const char* debug_name;
    struct RogueBTNode** children; // simple array of child pointers
    uint16_t child_count;
    uint16_t child_capacity;
    uint8_t state_u8; // small state storage for simple nodes (e.g., current child index)
    void* user_data; // optional custom payload
} RogueBTNode;

// Behavior tree root wrapper
typedef struct RogueBehaviorTree {
    RogueBTNode* root;
} RogueBehaviorTree;

// API
RogueBTNode* rogue_bt_node_create(const char* debug_name, uint16_t initial_capacity, RogueBTNodeTick tick_fn);
void rogue_bt_node_destroy(RogueBTNode* node); // recursive
bool rogue_bt_node_add_child(RogueBTNode* parent, RogueBTNode* child);

RogueBehaviorTree* rogue_behavior_tree_create(RogueBTNode* root);
void rogue_behavior_tree_destroy(RogueBehaviorTree* tree);
RogueBTStatus rogue_behavior_tree_tick(RogueBehaviorTree* tree, struct RogueBlackboard* bb, float dt);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_AI_BEHAVIOR_TREE_H
