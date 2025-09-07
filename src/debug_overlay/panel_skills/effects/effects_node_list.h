#ifndef EFFECTS_NODE_LIST_H
#define EFFECTS_NODE_LIST_H

#include "../../../core/skills/skill_debug.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Basic primary + node list sliders & CRUD. */
    int effects_node_list_editor_draw(int* primary_id, struct RogueSkillEffectNode* nodes,
                                      int* node_count); /* node_count updated when resized */

#ifdef __cplusplus
}
#endif

#endif /* EFFECTS_NODE_LIST_H */
