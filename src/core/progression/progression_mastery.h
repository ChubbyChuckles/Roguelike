/* Mastery System (Progression Phase 6.1–6.5)
 * Features Delivered:
 *  - 6.1: Per-skill mastery XP & rank thresholds (geometric growth)
 *  - 6.2: Minor passive ring points unlocked when a skill reaches a target rank
 *  - 6.3: Per-skill mastery bonus scalar tiers by rank bracket
 *  - 6.4: Optional decay / plateau mechanic (inactivity decays portion of surplus XP)
 *  - 6.5: Unit tests validate XP->rank growth, ring point counting, tier scaling & decay behavior
 * API below exposes both the new extended interface (rogue_mastery_*) and thin backwards-compat
 * wrappers retained for earlier simple tests (rogue_progression_mastery_*).
 */
#ifndef ROGUE_PROGRESSION_MASTERY_H
#define ROGUE_PROGRESSION_MASTERY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize mastery system for up to max_skills (will grow dynamically if exceeded when
     * max_skills>0). enable_decay: 1 enables inactivity decay logic. */
    int rogue_mastery_init(int max_skills, int enable_decay);
    void rogue_mastery_shutdown(void);

    /* Add mastery XP for a skill (skill usage event). `xp` is logical units (not yet scaled) and
     * timestamp_ms is the global progression clock (monotonic). Returns 0 on success, <0 on
     * failure. */
    int rogue_mastery_add_xp(int skill_id, unsigned int xp, unsigned int timestamp_ms);

    /* Advance time (call each frame or tick) with elapsed milliseconds to process decay
     * bookkeeping. */
    void rogue_mastery_update(unsigned int elapsed_ms);

    /* Query integer rank (0..), total XP, and XP to next rank for a skill. */
    unsigned short rogue_mastery_rank(int skill_id);
    unsigned long long rogue_mastery_xp(int skill_id);
    unsigned long long rogue_mastery_xp_to_next(int skill_id);

    /* Mastery bonus scalar (>=1.0) applied to allowed effect domains (damage, resource efficiency,
     * buildup). Tier mapping is intentionally coarse to ease balancing tweaks. */
    float rogue_mastery_bonus_scalar(int skill_id);

    /* Count distinct skills whose rank >= ring unlock threshold (minor ring currency). */
    int rogue_mastery_minor_ring_points(void);

    /* Enable / disable decay at runtime. */
    void rogue_mastery_set_decay(int enabled);

    /* ---- Backwards compatibility wrappers (Phase 6.1 minimal API) ---- */
    int rogue_progression_mastery_init(void);
    void rogue_progression_mastery_shutdown(void);
    double rogue_progression_mastery_add_xp(int skill_id, double xp);
    double rogue_progression_mastery_get_xp(int skill_id);
    int rogue_progression_mastery_get_rank(int skill_id);
    double rogue_progression_mastery_threshold_for_rank(int rank);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_PROGRESSION_MASTERY_H */
