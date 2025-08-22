/* Skill Specialization Paths (Integration Plumbing Phase 3.6.6–3.6.7)
 * Simple per-skill specialization choice with two canonical paths:
 *  - POWER (1): Boost damage output via multiplicative scalar.
 *  - CONTROL (2): Reduce cooldowns via multiplicative scalar.
 * Choices are stored per skill id; re-spec consumes a shared respec token
 * from the attribute progression pool (g_attr_state.respec_tokens).
 */
#ifndef ROGUE_PROGRESSION_SPECIALIZATION_H
#define ROGUE_PROGRESSION_SPECIALIZATION_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Path identifiers */
    enum
    {
        ROGUE_SPEC_NONE = 0,
        ROGUE_SPEC_POWER = 1,
        ROGUE_SPEC_CONTROL = 2,
    };

    /* Lifecycle */
    int rogue_specialization_init(int max_skills);
    void rogue_specialization_shutdown(void);

    /* Choose specialization path for a skill. Returns 0 success, <0 on error.
     * Errors: -1 invalid args, -2 already chosen, -3 out of memory. */
    int rogue_specialization_choose(int skill_id, int path_id);

    /* Get chosen path for a skill (0 if none). */
    int rogue_specialization_get(int skill_id);

    /* Respec specialization for a skill (clear choice). Consumes one respec token
     * from progression attributes. Returns 0 success, <0 on error.
     * Errors: -1 invalid args or none chosen, -2 no tokens. */
    int rogue_specialization_respec(int skill_id);

    /* Scalars applied by specialization. */
    float rogue_specialization_damage_scalar(int skill_id);
    float rogue_specialization_cooldown_scalar(int skill_id);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_PROGRESSION_SPECIALIZATION_H */
