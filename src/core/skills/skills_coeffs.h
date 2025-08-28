#ifndef ROGUE_CORE_SKILLS_COEFFS_H
#define ROGUE_CORE_SKILLS_COEFFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Phase 8: Central coefficient table keyed by skill id + rank with stat contributions.
       Coefficients produce a multiplicative damage scalar applied alongside mastery/specialization.
     */

    typedef struct RogueSkillCoeffParams
    {
        /* Base scalar at rank 1 (e.g., 1.0 = no change). */
        float base_scalar;
        /* Per-rank additive to scalar (rank>=2): eff = base + per_rank*(rank-1). */
        float per_rank_scalar;
        /* Stat contribution flags (percent per 10 points). Applied via soft-cap curve. */
        float str_pct_per10; /* Strength -> physical leaning */
        float int_pct_per10; /* Intelligence -> arcane leaning */
        float dex_pct_per10; /* Dexterity -> crit/evasion leaning (expected-value approx) */
        /* Soft cap config (applies to each stat contribution independently). */
        float stat_cap_pct;  /* e.g., 50 means contributions asymptotically approach +50% */
        float stat_softness; /* higher => slower approach to cap */
    } RogueSkillCoeffParams;

    /* Register or update coefficient params for a skill id. Returns 0 on success. */
    int rogue_skill_coeff_register(int skill_id, const RogueSkillCoeffParams* params);

    /* Compute the coefficient scalar for a skill id considering current rank and player stats.
       Returns 1.0f when no entry exists. Safe to call at any time. */
    float rogue_skill_coeff_get_scalar(int skill_id);

    /* Query: returns 1 if a coefficient entry exists for skill_id, 0 otherwise. */
    int rogue_skill_coeff_exists(int skill_id);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILLS_COEFFS_H */
