/* enemy_ai_intensity.c - Phase 5 AI Behavior Intensity Layers implementation */
#include "core/enemy/enemy_ai_intensity.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include <math.h>
#include <string.h>

/* Static profiles (could be data-driven later) */
static const RogueAIIntensityProfile g_profiles[ROGUE_AI_INTENSITY__COUNT] = {
    {0.80f, 0.90f, 1.10f}, /* Passive */
    {1.00f, 1.00f, 1.00f}, /* Standard */
    {1.25f, 1.15f, 0.85f}, /* Aggressive */
    {1.55f, 1.25f, 0.70f}  /* Frenzied */
};

const RogueAIIntensityProfile* rogue_ai_intensity_profile(RogueEnemyAIIntensity tier)
{
    if (tier < 0 || tier >= ROGUE_AI_INTENSITY__COUNT)
        return NULL;
    return &g_profiles[tier];
}

void rogue_ai_intensity_force(struct RogueEnemy* e, RogueEnemyAIIntensity tier)
{
    if (!e)
        return;
    if (tier < 0 || tier >= ROGUE_AI_INTENSITY__COUNT)
        return;
    e->ai_intensity = (unsigned char) tier;
    e->ai_intensity_score = (float) tier;
}

/* Escalation logic:
 * - Base score drifts toward 1 (Standard) when idle.
 * - Triggers increasing score: player low health (<35%), recent pack member deaths, close proximity
 * (<3 tiles), player high aggression (time since player hit small).
 * - De-escalation: player at high health (>80%), no proximity, elapsed calm time.
 * Score thresholds map to tiers:
 *   <0.5 => Passive, <1.5 => Standard, <2.5 => Aggressive, else Frenzied.
 * Hysteresis: intensity_cooldown_ms prevents tier change spam (min 1200ms between changes) and
 * score clamped inside [0,3.5].
 */
void rogue_ai_intensity_update(struct RogueEnemy* e, float dt_ms, int player_low_health,
                               int pack_deaths_recent)
{
    if (!e)
        return;
    if (dt_ms <= 0)
        return;
    float dt_s = dt_ms * 0.001f;
    /* Passive drift toward baseline */
    float target_base = 1.0f;
    e->ai_intensity_score += (target_base - e->ai_intensity_score) * (0.25f * dt_s);
    /* Proximity trigger */
    float pdx = g_app.player.base.pos.x - e->base.pos.x;
    float pdy = g_app.player.base.pos.y - e->base.pos.y;
    float dist2 = pdx * pdx + pdy * pdy;
    if (dist2 < 9.0f)
    {
        e->ai_intensity_score += 1.2f * dt_s;
    }
    if (dist2 < 2.0f)
    {
        e->ai_intensity_score += 1.8f * dt_s;
    }
    if (player_low_health)
        e->ai_intensity_score += 0.9f * dt_s;
    if (pack_deaths_recent)
        e->ai_intensity_score += 1.5f * dt_s; /* escalation trigger */
    /* Calm conditions */
    int player_high_health = (g_app.player.health > (int) (g_app.player.max_health * 0.8f));
    if (player_high_health && dist2 > 36.0f)
    {
        float decay = 1.6f * dt_s; /* stronger de-escalation when fully calm */
        if (e->ai_intensity == ROGUE_AI_INTENSITY_FRENZIED)
            decay *= 2.0f; /* accelerate exit from frenzied */
        e->ai_intensity_score -= decay;
    }
    /* Clamp */
    if (e->ai_intensity_score < 0.f)
        e->ai_intensity_score = 0.f;
    if (e->ai_intensity_score > 3.5f)
        e->ai_intensity_score = 3.5f;
    /* Hysteresis tier derivation */
    RogueEnemyAIIntensity new_tier;
    if (e->ai_intensity_score < 0.5f)
        new_tier = ROGUE_AI_INTENSITY_PASSIVE;
    else if (e->ai_intensity_score < 1.5f)
        new_tier = ROGUE_AI_INTENSITY_STANDARD;
    else if (e->ai_intensity_score < 2.5f)
        new_tier = ROGUE_AI_INTENSITY_AGGRESSIVE;
    else
        new_tier = ROGUE_AI_INTENSITY_FRENZIED;
    if (e->ai_intensity_cooldown_ms > 0)
        e->ai_intensity_cooldown_ms -= dt_ms;
    else
        e->ai_intensity_cooldown_ms = 0;
    if (new_tier != (RogueEnemyAIIntensity) e->ai_intensity && e->ai_intensity_cooldown_ms <= 0)
    {
        /* Apply change & reset cooldown */
        e->ai_intensity = (unsigned char) new_tier;
        e->ai_intensity_cooldown_ms = 1200.0f;
        /* Snap score slightly inside band to reduce thrash */
        switch (new_tier)
        {
        case ROGUE_AI_INTENSITY_PASSIVE:
            e->ai_intensity_score = 0.25f;
            break;
        case ROGUE_AI_INTENSITY_STANDARD:
            e->ai_intensity_score = 1.0f;
            break;
        case ROGUE_AI_INTENSITY_AGGRESSIVE:
            e->ai_intensity_score = 2.0f;
            break;
        case ROGUE_AI_INTENSITY_FRENZIED:
            e->ai_intensity_score = 3.0f;
            break;
        default:
            break;
        }
    }
}
