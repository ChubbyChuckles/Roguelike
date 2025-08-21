/* enemy_adaptive.h - Enemy Difficulty System Phase 4 (Adaptive Difficulty Feedback Loop)
 * Provides runtime performance KPI smoothing (EMA) and bounded difficulty scalar adjustments.
 * Ordering: Base -> Relative ΔL -> Tier/Modifiers -> Adaptive Scalar -> (future) Boss/Nemesis
 * scripts.
 */
#ifndef ROGUE_CORE_ENEMY_ADAPTIVE_H
#define ROGUE_CORE_ENEMY_ADAPTIVE_H
#ifdef __cplusplus
extern "C"
{
#endif

/* Public configuration constants */
#define ROGUE_ENEMY_ADAPTIVE_MIN_SCALAR 0.88f
#define ROGUE_ENEMY_ADAPTIVE_MAX_SCALAR 1.12f

    /* Initialize / reset state */
    void rogue_enemy_adaptive_reset(void);
    /* Enable/disable adaptive adjustments (disabled => scalar fixed at 1.0) */
    void rogue_enemy_adaptive_set_enabled(int enabled);
    int rogue_enemy_adaptive_enabled(void);

    /* Submit performance events (time values in seconds). */
    void rogue_enemy_adaptive_submit_kill(
        float ttk_seconds); /* Enemy killed with measured time-to-kill */
    void
    rogue_enemy_adaptive_submit_player_damage(float damage_amount,
                                              float interval_seconds); /* aggregate damage intake */
    void rogue_enemy_adaptive_submit_potion_used(void);
    void rogue_enemy_adaptive_submit_player_death(void);

    /* Advance smoothing & apply adjustment (call periodically, e.g., per second). */
    void rogue_enemy_adaptive_tick(float dt_seconds);

    /* Current scalar applied to enemy hp/damage/defense after tier & ΔL scaling. */
    float rogue_enemy_adaptive_scalar(void);

#ifdef __cplusplus
}
#endif
#endif
