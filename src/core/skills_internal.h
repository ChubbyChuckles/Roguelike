/* Internal shared declarations for skills module split */
#ifndef ROGUE_SKILLS_INTERNAL_H
#define ROGUE_SKILLS_INTERNAL_H

#include "core/skills.h"

extern struct RogueSkillDef* g_skill_defs_internal;
extern struct RogueSkillState* g_skill_states_internal;
extern int g_skill_capacity_internal;
extern int g_skill_count_internal;

/* Registry helpers */
void rogue_skills_recompute_synergies(void);
void rogue_skills_ensure_capacity(int min_cap);

#endif /* ROGUE_SKILLS_INTERNAL_H */
