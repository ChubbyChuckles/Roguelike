#ifndef ROGUE_UI_CONTEXT_INTERNAL_H
#define ROGUE_UI_CONTEXT_INTERNAL_H

#include "ui_context.h"

/* Internal helpers shared across UI context translation units. Not part of public API. */
static inline void rogue_ui_internal_assign_id(RogueUINode* n)
{
    if (n && n->text)
        n->id_hash = rogue_ui_make_id(n->text);
}
static inline int rogue_ui_internal_push_node(RogueUIContext* ctx, RogueUINode n)
{
    if (!ctx || !ctx->nodes || ctx->node_count >= ctx->node_capacity)
        return -1;
    if (n.parent_index < -1)
        n.parent_index = -1;
    ctx->nodes[ctx->node_count] = n;
    ctx->node_count++;
    ctx->stats.node_count = ctx->node_count;
    return ctx->node_count - 1;
}

#endif /* ROGUE_UI_CONTEXT_INTERNAL_H */
