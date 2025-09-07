#ifndef EFFECTS_TREE_EDITOR_H
#define EFFECTS_TREE_EDITOR_H

#include "../../../core/skills/skill_debug.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Draws the experimental effect tree editor. Mutates tree_nodes/tree_count.
     * Returns non-zero if any data was changed that requires persistence. */
    int effects_tree_editor_draw(int skill_index, const char* overrides_path,
                                 struct RogueSkillEffectTreeNodeDebug* tree_nodes, int* tree_count);

#ifdef __cplusplus
}
#endif

#endif /* EFFECTS_TREE_EDITOR_H */
