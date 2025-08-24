/* enemy_modifiers.h - Enemy Difficulty System Phase 2 (procedural enemy modifiers)
 * Roadmap Phase 2 Coverage: 2.1 - 2.5
 */
#ifndef ROGUE_CORE_ENEMY_MODIFIERS_H
#define ROGUE_CORE_ENEMY_MODIFIERS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "enemy_difficulty.h"

#define ROGUE_ENEMY_MAX_MODIFIERS 64
#define ROGUE_ENEMY_MAX_ACTIVE_MODS 8

    /* Tier bitmask (bit = tier id) */
    typedef unsigned int RogueEnemyTierMask;

    typedef struct RogueEnemyModifierDef
    {
        int id;                   /* numeric id */
        const char* name;         /* interned name */
        float weight;             /* selection weight */
        RogueEnemyTierMask tiers; /* allowed tiers */
        float dps_cost;           /* budget consumption fractions (<=1) */
        float control_cost;
        float mobility_cost;
        unsigned int incompat_mask; /* bitmask of incompatible defs (index based) */
        const char* telegraph;      /* aura/icon identifier */
    } RogueEnemyModifierDef;

    typedef struct RogueEnemyModifierSet
    {
        int count;
        const RogueEnemyModifierDef* defs[ROGUE_ENEMY_MAX_ACTIVE_MODS];
        float total_dps_cost;
        float total_control_cost;
        float total_mobility_cost;
        unsigned int applied_mask; /* bit per modifier index */
    } RogueEnemyModifierSet;

    /* Load modifiers from key/value config file path. Returns number loaded or <0 on error. */
    int rogue_enemy_modifiers_load_file(const char* path);
    /* Reset registry (frees names). */
    void rogue_enemy_modifiers_reset(void);
    /* Introspection */
    int rogue_enemy_modifier_count(void);
    const RogueEnemyModifierDef* rogue_enemy_modifier_at(int index);
    const RogueEnemyModifierDef* rogue_enemy_modifier_by_id(int id);

    /* Roll deterministic set for enemy of a given tier.
     * seed: deterministic stream (encounter_seed ^ enemy_id etc externally)
     * tier_id: enemy tier id
     * out_max_fraction: optional cap per budget dimension (e.g., 0.6f) (if <=0 => defaults to 0.6)
     * Returns 0 on success.
     */
    int rogue_enemy_modifiers_roll(unsigned int seed, int tier_id, float out_max_fraction,
                                   RogueEnemyModifierSet* out);

    /* Deterministic utility for tests: same parameters => same sequence. */
    unsigned int rogue_enemy_modifiers_rng_next(unsigned int* state);
    int rogue_enemy_modifiers_rng_range(unsigned int* state, int hi_exclusive);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_ENEMY_MODIFIERS_H */
