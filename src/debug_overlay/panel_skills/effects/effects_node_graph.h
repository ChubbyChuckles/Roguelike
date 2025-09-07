#ifndef EFFECTS_NODE_GRAPH_H
#define EFFECTS_NODE_GRAPH_H

#include "../../../core/skills/skill_debug.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Legacy mini node graph visualization/editor (pre-tree). */
    int effects_node_graph_editor_draw(int skill_index, int* primary_id,
                                       struct RogueSkillEffectNode* nodes, int node_count);

#ifdef __cplusplus
}
#endif

#endif /* EFFECTS_NODE_GRAPH_H */
