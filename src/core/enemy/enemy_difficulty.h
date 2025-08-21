/* enemy_difficulty.h - Enemy Difficulty System Phase 0 (taxonomy + tier multipliers)
 *
 * Phase 0 Goals (Roadmap 0.1 - 0.5):
 *  0.1 Inventory existing enemy archetypes (melee, ranged, caster, elite, boss)
 *  0.2 Define canonical difficulty attributes: hp_budget, dps_budget, control_budget,
 * mobility_budget 0.3 Establish tier bands (Normal, Veteran, Elite, MiniBoss, Boss, Nemesis) with
 * budget multipliers 0.4 Reserve ID ranges for future special tiers (Mythic, Event) (not populated
 * yet) 0.5 Tests: uniqueness, multiplier table monotonicity
 *
 * This header exposes a minimal, stable C API so later phases (baseline scaling,
 * relative level differentials, modifiers, encounter composition) can build on
 * a consistent taxonomy without churn.
 */
#ifndef ROGUE_CORE_ENEMY_DIFFICULTY_H
#define ROGUE_CORE_ENEMY_DIFFICULTY_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Enemy archetype high‑level behavioral classes.
     * (Future phases may attach default AI profiles / behavior tree roots.) */
    typedef enum RogueEnemyArchetype
    {
        ROGUE_ENEMY_ARCHETYPE_MELEE = 0,
        ROGUE_ENEMY_ARCHETYPE_RANGED = 1,
        ROGUE_ENEMY_ARCHETYPE_CASTER = 2,
        ROGUE_ENEMY_ARCHETYPE_ELITE_SUPPORT = 3, /* elite utility (auras, buffs) */
        ROGUE_ENEMY_ARCHETYPE_BOSS = 4,
        ROGUE_ENEMY_ARCHETYPE__COUNT
    } RogueEnemyArchetype;

    /* Canonical budget attribute bundle (Phase 0 definition). Units are relative,
     * later phases map these into concrete stat curves & ceilings. */
    typedef struct RogueEnemyDifficultyBudgets
    {
        float hp_budget;       /* Relative survivability allocation */
        float dps_budget;      /* Offensive output allocation */
        float control_budget;  /* Crowd control / disruption allocation */
        float mobility_budget; /* Movement / reposition / gap‑close allocation */
    } RogueEnemyDifficultyBudgets;

    /* Tier descriptor. Multipliers apply to a conceptual BASE budget (1.0).
     * Later phases will supply base curves -> apply tier multiplier -> relative ΔL model ->
     * modifiers -> adaptive. */
    typedef struct RogueEnemyTierDesc
    {
        int id;                           /* Stable tier id (dense 0..N for active tiers) */
        const char* name;                 /* Human readable (used by UI / analytics) */
        RogueEnemyDifficultyBudgets mult; /* Budget multipliers */
        unsigned int flags;               /* Reserved for future (e.g., boss phase scripts) */
    } RogueEnemyTierDesc;

    /* Active tier IDs (Phase 0) */
    enum
    {
        ROGUE_ENEMY_TIER_NORMAL = 0,
        ROGUE_ENEMY_TIER_VETERAN = 1,
        ROGUE_ENEMY_TIER_ELITE = 2,
        ROGUE_ENEMY_TIER_MINIBOSS = 3,
        ROGUE_ENEMY_TIER_BOSS = 4,
        ROGUE_ENEMY_TIER_NEMESIS = 5
    };

/* Reserved ID bases for future expansion (Phase 0 – reservation only) */
#define ROGUE_ENEMY_TIER_ID_MYTHIC_BASE 1000
#define ROGUE_ENEMY_TIER_ID_EVENT_BASE 2000

    /* Introspection API */
    int rogue_enemy_tier_count(void);                                   /* number of active tiers */
    const RogueEnemyTierDesc* rogue_enemy_tier_get_by_index(int index); /* NULL on OOB */
    const RogueEnemyTierDesc* rogue_enemy_tier_get(int id);             /* NULL if not found */

    /* Compute the base (tier * base=1) budgets for a tier. Returns 0 on success. */
    int rogue_enemy_difficulty_compute_base_budgets(int tier_id, RogueEnemyDifficultyBudgets* out);

    /* Archetype enumeration helper */
    int rogue_enemy_archetype_count(void); /* returns ROGUE_ENEMY_ARCHETYPE__COUNT */
    const char* rogue_enemy_archetype_name(int archetype); /* NULL if invalid */

    /* Test helper / future dynamic registry reset (currently trivial). */
    void rogue_enemy_difficulty_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_ENEMY_DIFFICULTY_H */
