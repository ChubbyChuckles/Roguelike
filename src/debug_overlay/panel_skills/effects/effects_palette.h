#ifndef EFFECTS_PALETTE_H
#define EFFECTS_PALETTE_H

#include "../../../core/skills/skill_debug.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* EffectSpec palette with filtering & assignment. */
    int effects_palette_draw(int* primary_id, struct RogueSkillEffectNode* nodes, int node_count);

#ifdef __cplusplus
}
#endif

#endif /* EFFECTS_PALETTE_H */
