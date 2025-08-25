/**
 * @file behavior_tree.c
 * @brief Simple behavior tree node & tree utilities.
 *
 * Provides node allocation, destruction, basic dynamic child array growth,
 * tree lifecycle management, ticking, and a utility to serialize the active
 * path into a compact string form. The implementation is intentionally
 * minimal and suitable for deterministic unit tests and small AI trees.
 */
#include "behavior_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global tick index used to stamp nodes marked during a tree tick
static uint32_t g_bt_current_tick = 0;

/**
 * @brief Create a new behavior tree node.
 *
 * Allocates and zero-initializes a RogueBTNode and its vtable, sets the
 * provided tick function and debug name, and optionally preallocates the
 * children pointer array to initial_capacity entries.
 *
 * @param debug_name Optional debug name for human-readable dumps; pointer is stored (not copied).
 * @param initial_capacity Initial child capacity to preallocate (0 allowed).
 * @param tick_fn Tick function pointer stored in the node vtable.
 * @return RogueBTNode* Newly allocated node or NULL on allocation failure.
 */
RogueBTNode* rogue_bt_node_create(const char* debug_name, uint16_t initial_capacity,
                                  RogueBTNodeTick tick_fn)
{
    RogueBTNode* n = (RogueBTNode*) calloc(1, sizeof(RogueBTNode));
    if (!n)
        return NULL;
    RogueBTNodeVTable* vt = (RogueBTNodeVTable*) calloc(1, sizeof(RogueBTNodeVTable));
    if (!vt)
    {
        free(n);
        return NULL;
    }
    vt->tick = tick_fn;
    n->vtable = vt;
    n->debug_name = debug_name;
    n->child_capacity = initial_capacity;
    n->user_data_dtor = NULL;
    if (initial_capacity)
    {
        n->children = (RogueBTNode**) calloc(initial_capacity, sizeof(RogueBTNode*));
    }
    return n;
}

/* Optional external cleanup symbol (weak-like). We'll look it up via extern; if missing, it's fine.
 */
/* Forward declaration; advanced_nodes.c provides implementation. Provide weak fallback via macro if
 * not linked. */
/**
 * @brief Optional per-node advanced cleanup hook.
 *
 * Some advanced node implementations may require specialized cleanup. If
 * linked, advanced_nodes.c should provide this symbol. A no-op fallback is
 * provided when ROGUE_NO_ADVANCED_NODES is defined.
 *
 * @param node Node to clean up.
 */
void rogue_bt_advanced_cleanup(RogueBTNode* node);
#ifdef ROGUE_NO_ADVANCED_NODES
void rogue_bt_advanced_cleanup(RogueBTNode* node) { (void) node; }
#endif

/**
 * @brief Recursively destroy a node and its children.
 *
 * Performs a post-order traversal, invoking advanced cleanup for the node
 * (no-op if fallback), freeing child arrays, the vtable, and finally the
 * node itself. NULL input is ignored.
 *
 * @param node Root of the subtree to destroy.
 */
void rogue_bt_node_destroy(RogueBTNode* node)
{
    if (!node)
        return;
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        rogue_bt_node_destroy(node->children[i]);
    }
    /* Free node-owned user_data if a destructor is provided */
    if (node->user_data && node->user_data_dtor)
    {
        node->user_data_dtor(node->user_data);
        node->user_data = NULL;
    }
    /* Call advanced cleanup (may be no-op if fallback) */
    rogue_bt_advanced_cleanup(node);
    free(node->children);
    free((void*) node->vtable);
    free(node);
}

/**
 * @brief Add a child node to a parent, growing the child array as needed.
 *
 * If the parent's child array is full, it grows the capacity (double up
 * to initial fallback of 4). Returns true on success and false on OOM or
 * invalid inputs.
 *
 * @param parent Parent node to append the child to.
 * @param child Child node to add.
 * @return bool True on success, false on failure.
 */
bool rogue_bt_node_add_child(RogueBTNode* parent, RogueBTNode* child)
{
    if (!parent || !child)
        return false;
    if (parent->child_count == parent->child_capacity)
    {
        uint16_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
        RogueBTNode** new_children =
            (RogueBTNode**) realloc(parent->children, new_cap * sizeof(RogueBTNode*));
        if (!new_children)
            return false;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return true;
}

/**
 * @brief Create a behavior tree wrapper for a given root node.
 *
 * Allocates a RogueBehaviorTree and assigns the provided root. Returns NULL
 * on invalid input or allocation failure.
 *
 * @param root Root node for the new tree.
 * @return RogueBehaviorTree* Pointer to the created tree or NULL.
 */
RogueBehaviorTree* rogue_behavior_tree_create(RogueBTNode* root)
{
    if (!root)
        return NULL;
    RogueBehaviorTree* t = (RogueBehaviorTree*) calloc(1, sizeof(RogueBehaviorTree));
    if (!t)
        return NULL;
    t->root = root;
    return t;
}

/**
 * @brief Destroy a behavior tree and free associated memory.
 *
 * Destroys the entire node subtree rooted at tree->root and frees the
 * tree wrapper itself. NULL tree is ignored.
 *
 * @param tree Tree to destroy.
 */
void rogue_behavior_tree_destroy(RogueBehaviorTree* tree)
{
    if (!tree)
        return;
    rogue_bt_node_destroy(tree->root);
    free(tree);
}

/**
 * @brief Tick the behavior tree by invoking the root node's tick function.
 *
 * Validates the tree & root tick function, increments the tick counter, and
 * forwards the call to the root vtable tick function. Returns
 * ROGUE_BT_INVALID on invalid inputs.
 *
 * @param tree Behavior tree to tick.
 * @param bb Optional blackboard passed to the tick function.
 * @param dt Delta time in seconds.
 * @return RogueBTStatus Status returned by the root tick or ROGUE_BT_INVALID.
 */
RogueBTStatus rogue_behavior_tree_tick(RogueBehaviorTree* tree, struct RogueBlackboard* bb,
                                       float dt)
{
    if (!tree || !tree->root || !tree->root->vtable || !tree->root->vtable->tick)
        return ROGUE_BT_INVALID;
    tree->tick_count++;
    g_bt_current_tick = tree->tick_count;
    return tree->root->vtable->tick(tree->root, bb, dt);
}

void rogue_bt_mark_node(RogueBTNode* node, RogueBTStatus st)
{
    if (!node)
        return;
    node->last_status = st;
    node->last_tick = g_bt_current_tick;
}

static void serialize_path_recursive(RogueBTNode* node, char** cursor, char* end,
                                     int* first_written, uint32_t current_tick)
{
    if (!node)
        return;
    if (*cursor >= end)
        return;
    // Only include nodes that ran this tick and returned SUCCESS or RUNNING
    if (node->last_tick == current_tick &&
        (node->last_status == ROGUE_BT_SUCCESS || node->last_status == ROGUE_BT_RUNNING))
    {
        int remaining = (int) (end - *cursor);
        if (!*first_written)
        {
            int w = snprintf(*cursor, remaining, "%s", node->debug_name ? node->debug_name : "?");
            if (w > 0)
                *cursor += (w < remaining ? w : remaining);
            *first_written = 1;
        }
        else
        {
            int w = snprintf(*cursor, remaining, ">%s", node->debug_name ? node->debug_name : "?");
            if (w > 0)
                *cursor += (w < remaining ? w : remaining);
        }
    }
    for (uint16_t i = 0; i < node->child_count; i++)
    {
        serialize_path_recursive(node->children[i], cursor, end, first_written, current_tick);
    }
}

/**
 * @brief Serialize the active path (pre-order names) into a buffer.
 *
 * Walks the tree and concatenates node debug names separated by '>' into the
 * provided buffer. Returns -1 on invalid input; otherwise returns the number
 * of bytes written excluding the trailing NUL.
 *
 * @param tree Tree to serialize.
 * @param out Output buffer to receive the serialized path.
 * @param max_out Size of the output buffer in bytes (must be > 0).
 * @return int Number of bytes written (excluding NUL) or -1 on invalid args.
 */
int rogue_behavior_tree_serialize_active_path(RogueBehaviorTree* tree, char* out, int max_out)
{
    if (!tree || !out || max_out <= 0)
        return -1;
    char* cursor = out;
    char* end = out + (max_out - 1);
    int first = 0;
    // Before serializing, propagate current tick into nodes touched this frame by a pre-walk
    // We'll update last_tick during this walk when we see RUNNING/SUCCESS states
    // Since we don't keep a list, we mark during ticking paths below (see basic/advanced nodes)
    // Use tree->tick_count as the current tick index
    // Now serialize nodes that match current tick
    serialize_path_recursive(tree->root, &cursor, end, &first, tree->tick_count);
    *cursor = '\0';
    return (int) (cursor - out);
}
