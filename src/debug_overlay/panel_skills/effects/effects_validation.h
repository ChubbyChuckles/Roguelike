#ifndef EFFECTS_VALIDATION_H
#define EFFECTS_VALIDATION_H

#include "../../../core/skills/skill_debug.h"
#include "../../../graphics/effect_spec.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Performs inline validation UI for legacy nodes & primary id.
     * Returns non-zero if a change was made via quick-fix buttons. */
    int effects_validation_draw(int* primary_id, struct RogueSkillEffectNode* nodes,
                                int node_count);

#ifdef __cplusplus
}
#endif

#endif /* EFFECTS_VALIDATION_H */
