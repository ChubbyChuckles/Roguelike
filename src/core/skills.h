#ifndef ROGUE_CORE_SKILLS_H
#define ROGUE_CORE_SKILLS_H
#include <stdint.h>

/* Forward declares */
struct RogueSkillDef;
struct RogueSkillState;

/* Skill activation context */
typedef struct RogueSkillCtx {
    double now_ms; /* global time (ms) */
    int player_level;
    int talent_points; /* remaining */
} RogueSkillCtx;

/* Effect callback (optional). Return 1 if activation consumed resources. */
typedef int (*RogueSkillEffectFn)(const struct RogueSkillDef* def, struct RogueSkillState* st, const RogueSkillCtx* ctx);

/* Definition (immutable) */
typedef struct RogueSkillDef {
    int id;                     /* index into registry */
    const char* name;           /* display name */
    const char* icon;           /* icon path (future) */
    int max_rank;               /* maximum rank */
    float base_cooldown_ms;     /* base cooldown at rank 1 */
    float cooldown_reduction_ms_per_rank; /* linear reduction */
    RogueSkillEffectFn on_activate;
} RogueSkillDef;

/* Player-owned state */
typedef struct RogueSkillState {
    int rank;           /* 0 = locked/unlearned */
    double cooldown_end_ms;
    int uses;           /* total uses lifetime (for tests/metrics) */
} RogueSkillState;

/* Lifecycle */
void rogue_skills_init(void);
void rogue_skills_shutdown(void);

/* Registration (call during init before gameplay) */
int rogue_skill_register(const RogueSkillDef* def); /* returns id */

/* Rank management */
int rogue_skill_rank_up(int id); /* returns new rank or -1 */

/* Activation */
int rogue_skill_try_activate(int id, const RogueSkillCtx* ctx); /* 1 success, 0 fail */

/* Frame update (cooldowns etc) */
void rogue_skills_update(double now_ms);

/* Query */
const RogueSkillDef* rogue_skill_get_def(int id);
const struct RogueSkillState* rogue_skill_get_state(int id);

#endif
