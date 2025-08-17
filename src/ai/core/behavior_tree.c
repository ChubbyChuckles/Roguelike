#include "behavior_tree.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

RogueBTNode* rogue_bt_node_create(const char* debug_name, uint16_t initial_capacity, RogueBTNodeTick tick_fn) {
    RogueBTNode* n = (RogueBTNode*)calloc(1, sizeof(RogueBTNode));
    if(!n) return NULL;
    RogueBTNodeVTable* vt = (RogueBTNodeVTable*)calloc(1, sizeof(RogueBTNodeVTable));
    if(!vt){ free(n); return NULL; }
    vt->tick = tick_fn;
    n->vtable = vt;
    n->debug_name = debug_name;
    n->child_capacity = initial_capacity;
    if(initial_capacity) {
        n->children = (RogueBTNode**)calloc(initial_capacity, sizeof(RogueBTNode*));
    }
    return n;
}

/* Optional external cleanup symbol (weak-like). We'll look it up via extern; if missing, it's fine. */
/* Forward declaration; advanced_nodes.c provides implementation. Provide weak fallback via macro if not linked. */
void rogue_bt_advanced_cleanup(RogueBTNode* node);
#ifdef ROGUE_NO_ADVANCED_NODES
void rogue_bt_advanced_cleanup(RogueBTNode* node) { (void)node; }
#endif

void rogue_bt_node_destroy(RogueBTNode* node) {
    if(!node) return;
    for(uint16_t i=0;i<node->child_count;i++) {
        rogue_bt_node_destroy(node->children[i]);
    }
    /* Call advanced cleanup (no-op if fallback) */
    rogue_bt_advanced_cleanup(node);
    free(node->children);
    free((void*)node->vtable);
    free(node);
}

bool rogue_bt_node_add_child(RogueBTNode* parent, RogueBTNode* child) {
    if(!parent || !child) return false;
    if(parent->child_count == parent->child_capacity) {
        uint16_t new_cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
        RogueBTNode** new_children = (RogueBTNode**)realloc(parent->children, new_cap * sizeof(RogueBTNode*));
        if(!new_children) return false;
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    parent->children[parent->child_count++] = child;
    return true;
}

RogueBehaviorTree* rogue_behavior_tree_create(RogueBTNode* root) {
    if(!root) return NULL;
    RogueBehaviorTree* t = (RogueBehaviorTree*)calloc(1, sizeof(RogueBehaviorTree));
    if(!t) return NULL;
    t->root = root;
    return t;
}

void rogue_behavior_tree_destroy(RogueBehaviorTree* tree) {
    if(!tree) return;
    rogue_bt_node_destroy(tree->root);
    free(tree);
}

RogueBTStatus rogue_behavior_tree_tick(RogueBehaviorTree* tree, struct RogueBlackboard* bb, float dt) {
    if(!tree || !tree->root || !tree->root->vtable || !tree->root->vtable->tick) return ROGUE_BT_INVALID;
    tree->tick_count++;
    return tree->root->vtable->tick(tree->root, bb, dt);
}

static void serialize_path_recursive(RogueBTNode* node, char** cursor, char* end, int* first_written) {
    if(!node) return;
    if(*cursor >= end) return;
    int remaining = (int)(end - *cursor);
    if(!*first_written) {
        int w = snprintf(*cursor, remaining, "%s", node->debug_name?node->debug_name:"?");
        if(w>0) *cursor += (w < remaining ? w : remaining);
        *first_written = 1;
    } else {
        int w = snprintf(*cursor, remaining, ">%s", node->debug_name?node->debug_name:"?");
        if(w>0) *cursor += (w < remaining ? w : remaining);
    }
    for(uint16_t i=0;i<node->child_count;i++) {
        serialize_path_recursive(node->children[i], cursor, end, first_written);
    }
}

int rogue_behavior_tree_serialize_active_path(RogueBehaviorTree* tree, char* out, int max_out) {
    if(!tree || !out || max_out<=0) return -1;
    char* cursor = out; char* end = out + (max_out-1); int first=0;
    serialize_path_recursive(tree->root, &cursor, end, &first);
    *cursor='\0';
    return (int)(cursor - out);
}
