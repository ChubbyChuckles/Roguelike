/* enemy_difficulty_scaling.h - Enemy Difficulty System Phase 1 (baseline scaling + relative level differential)
 * Roadmap Coverage: 1.1 - 1.7
 */
#ifndef ROGUE_CORE_ENEMY_DIFFICULTY_SCALING_H
#define ROGUE_CORE_ENEMY_DIFFICULTY_SCALING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/enemy_difficulty.h"

/* Parameter set loaded from data file (cfg key=value). */
typedef struct RogueEnemyDifficultyParams {
    float d_def;       /* per level downward defense/HP reduction coefficient (ΔL>0) */
    float d_dmg;       /* per level downward damage reduction coefficient (ΔL>0) */
    float cap_def;     /* max downward reduction applied to defense/HP */
    float cap_dmg;     /* max downward reduction applied to damage */
    float u_def;       /* per level upward defense/HP increase (ΔL<0) */
    float u_dmg;       /* per level upward damage increase (ΔL<0) */
    float u_cap_def;   /* cap on upward defense/HP increase */
    float u_cap_dmg;   /* cap on upward damage increase */
    float ramp_soft;   /* soft ramp offset before upward scaling accumulates */
    int   dominance_threshold; /* ΔL >= threshold -> dominance (cap reached) */
    int   trivial_threshold;   /* ΔL >= threshold -> reward dampening path */
    float reward_trivial_scalar; /* reward scalar at/after trivial threshold */
} RogueEnemyDifficultyParams;

/* Stats triple used by scaling API. */
typedef struct RogueEnemyBaseStats { float hp; float damage; float defense; } RogueEnemyBaseStats;

typedef struct RogueEnemyFinalStats { float hp; float damage; float defense; float hp_mult; float dmg_mult; float def_mult; } RogueEnemyFinalStats;

/* Load params from file (returns 0 on success). Missing file => keep defaults. */
int rogue_enemy_difficulty_load_params_file(const char* path);
/* Get current params (pointer to internal immutable instance). */
const RogueEnemyDifficultyParams* rogue_enemy_difficulty_params_current(void);
/* Reset to compile-time defaults. */
void rogue_enemy_difficulty_params_reset(void);

/* Baseline sublinear scaling curves (before tier & ΔL). */
float rogue_enemy_base_hp(int enemy_level);
float rogue_enemy_base_damage(int enemy_level);
float rogue_enemy_base_defense(int enemy_level);

/* Convenience: pack base stats for a level. */
RogueEnemyBaseStats rogue_enemy_base_stats(int enemy_level);

/* Apply tier multipliers (Phase 0) + relative ΔL model (Phase 1) producing final stats.
 * Returns 0 on success. player_level >=1, enemy_level>=1.
 */
int rogue_enemy_compute_final_stats(int player_level, int enemy_level, int tier_id, RogueEnemyFinalStats* out);

/* Reward scalar (Phase 1 basic): returns scalar in [reward_trivial_scalar, 1.0].
 * modifier_complexity_score, adaptive_state currently ignored (future phases). */
float rogue_enemy_compute_reward_scalar(int player_level, int enemy_level, float modifier_complexity_score, float adaptive_state_scalar);

/* Test helpers */
int rogue_enemy_difficulty_internal__relative_multipliers(int player_level, int enemy_level, float* out_hp_mult, float* out_dmg_mult);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_ENEMY_DIFFICULTY_SCALING_H */
