/* enemy_ai_intensity.h - Phase 5 AI Behavior Intensity Layers
 * Implements tiers Passive, Standard, Aggressive, Frenzied with escalation & de-escalation
 * triggers.
 */
#ifndef ROGUE_CORE_ENEMY_AI_INTENSITY_H
#define ROGUE_CORE_ENEMY_AI_INTENSITY_H

struct RogueEnemy; /* forward */

typedef enum RogueEnemyAIIntensity
{
    ROGUE_AI_INTENSITY_PASSIVE = 0,
    ROGUE_AI_INTENSITY_STANDARD,
    ROGUE_AI_INTENSITY_AGGRESSIVE,
    ROGUE_AI_INTENSITY_FRENZIED,
    ROGUE_AI_INTENSITY__COUNT
} RogueEnemyAIIntensity;

/* Public configuration for intensity multipliers (action frequency / move speed / cooldown scalar).
 */
typedef struct RogueAIIntensityProfile
{
    float action_freq_mult; /* attack attempt cadence multiplier */
    float move_speed_mult;  /* movement speed multiplier */
    float cooldown_mult;    /* attack cooldown scaling (lower => faster attacks) */
} RogueAIIntensityProfile;

const RogueAIIntensityProfile* rogue_ai_intensity_profile(RogueEnemyAIIntensity tier);

/* Runtime update: adjusts intensity score based on environmental & combat triggers. */
void rogue_ai_intensity_update(struct RogueEnemy* e, float dt_ms, int player_low_health,
                               int pack_deaths_recent);

/* Force set (used by tests) */
void rogue_ai_intensity_force(struct RogueEnemy* e, RogueEnemyAIIntensity tier);

#endif
