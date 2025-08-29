/**
 * @file combat_reaction.c
 * @brief Player reaction system for hit responses and invincibility frames
 *
 * This module implements the player's reaction system for handling damage responses,
 * including hit reactions (staggers, knockdowns), directional influence during reactions,
 * and invincibility frame management.
 *
 * Key features:
 * - Four reaction types with different durations and properties
 * - Directional influence (DI) system for reaction movement
 * - Early reaction cancellation windows
 * - Invincibility frame (iframe) management
 * - Automatic reaction cleanup and timer management
 *
 * Reaction Types:
 * - Type 1: Light reaction (220ms, 35% max DI)
 * - Type 2: Medium reaction (600ms, 55% max DI)
 * - Type 3: Heavy reaction (900ms, 85% max DI)
 * - Type 4: Severe reaction (1100ms, 100% max DI)
 *
 * The system provides controlled player responses to damage while allowing
 * limited player influence through directional inputs during reactions.
 */

#include "combat.h"
#include <math.h>
/**
 * @brief Initialize reaction parameters based on reaction type
 *
 * Sets up the directional influence limits and resets accumulation values
 * based on the current reaction type. Each reaction type has different
 * maximum directional influence allowed during the reaction.
 *
 * Directional Influence Limits by Type:
 * - Type 1: 35% maximum influence (light reactions)
 * - Type 2: 55% maximum influence (medium reactions)
 * - Type 3: 85% maximum influence (heavy reactions)
 * - Type 4: 100% maximum influence (severe reactions)
 * - Default: 0% influence (no reaction)
 *
 * @param p Pointer to the player entity (must not be NULL)
 *
 * @note Directional influence affects how much player input can influence reaction movement
 * @note Higher reaction types allow more player control during reactions
 * @note Accumulation values are reset to zero for fresh reaction start
 */
static void rogue_player_init_reaction_params(RoguePlayer* p)
{
    switch (p->reaction_type)
    {
    case 1:
        p->reaction_di_max = 0.35f;
        break;
    case 2:
        p->reaction_di_max = 0.55f;
        break;
    case 3:
        p->reaction_di_max = 0.85f;
        break;
    case 4:
        p->reaction_di_max = 1.00f;
        break;
    default:
        p->reaction_di_max = 0.0f;
        break;
    }
    p->reaction_di_accum_x = 0;
    p->reaction_di_accum_y = 0;
    p->reaction_canceled_early = 0;
}
/**
 * @brief Update reaction timers and invincibility frames
 *
 * Updates all reaction-related timers including reaction duration and
 * invincibility frames. Automatically cleans up expired reactions and
 * resets associated state variables.
 *
 * Timer Management:
 * - Reaction timer: Counts down reaction duration
 * - I-frame timer: Counts down invincibility duration
 *
 * When reaction timer expires:
 * - Reaction type is reset to 0 (no reaction)
 * - Total reaction time is cleared
 * - Directional influence accumulators are reset
 * - Maximum directional influence is reset
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param dt_ms Time delta in milliseconds since last update
 *
 * @note Reaction cleanup happens automatically when timer expires
 * @note I-frames are independent of reaction state
 * @note All timers are clamped to prevent negative values
 */
void rogue_player_update_reactions(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return;
    if (p->reaction_timer_ms > 0)
    {
        p->reaction_timer_ms -= dt_ms;
        if (p->reaction_timer_ms <= 0)
        {
            p->reaction_timer_ms = 0;
            p->reaction_type = 0;
            p->reaction_total_ms = 0;
            p->reaction_di_accum_x = 0;
            p->reaction_di_accum_y = 0;
            p->reaction_di_max = 0;
        }
    }
    if (p->iframes_ms > 0)
    {
        p->iframes_ms -= dt_ms;
        if (p->iframes_ms < 0)
            p->iframes_ms = 0;
    }
}
/**
 * @brief Apply a reaction state to the player
 *
 * Initiates a reaction state with the specified type, setting appropriate
 * timers and initializing directional influence parameters. Reactions
 * represent controlled responses to damage or other combat events.
 *
 * Reaction Type Durations:
 * - Type 1: 220ms (light reaction - quick stagger)
 * - Type 2: 600ms (medium reaction - moderate stagger)
 * - Type 3: 900ms (heavy reaction - strong stagger)
 * - Type 4: 1100ms (severe reaction - knockdown)
 * - Default: 300ms (fallback duration)
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param reaction_type Reaction type to apply (1-4, 0 to clear)
 *
 * @note Reaction type 0 or negative values are ignored
 * @note Total reaction time is stored for cancellation window calculations
 * @note Directional influence parameters are initialized based on reaction type
 * @note Existing reaction state is overwritten by new reaction
 */
void rogue_player_apply_reaction(RoguePlayer* p, int reaction_type)
{
    if (!p)
        return;
    if (reaction_type <= 0)
        return;
    p->reaction_type = reaction_type;
    switch (reaction_type)
    {
    case 1:
        p->reaction_timer_ms = 220.0f;
        break;
    case 2:
        p->reaction_timer_ms = 600.0f;
        break;
    case 3:
        p->reaction_timer_ms = 900.0f;
        break;
    case 4:
        p->reaction_timer_ms = 1100.0f;
        break;
    default:
        p->reaction_timer_ms = 300.0f;
        break;
    }
    p->reaction_total_ms = p->reaction_timer_ms;
    rogue_player_init_reaction_params(p);
}
/**
 * @brief Attempt to cancel the current reaction early
 *
 * Allows the player to cancel an ongoing reaction within specific timing windows,
 * providing a skill-based mechanic for reaction recovery. Each reaction type
 * has different cancellation windows based on elapsed time as a fraction of
 * total reaction duration.
 *
 * Cancellation Windows by Reaction Type:
 * - Type 1: 40-75% of duration (late in light reaction)
 * - Type 2: 55-85% of duration (mid-to-late in medium reaction)
 * - Type 3: 60-80% of duration (mid in heavy reaction)
 * - Type 4: 65-78% of duration (mid in severe reaction)
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @return 1 if reaction was successfully canceled, 0 if cancellation failed
 *
 * @note Can only cancel if currently in a reaction state
 * @note Cannot cancel if already canceled early in this reaction
 * @note Successful cancellation immediately ends the reaction
 * @note Sets early cancellation flag to prevent multiple cancels
 * @note Timing is based on fraction of total reaction duration elapsed
 */
int rogue_player_try_reaction_cancel(RoguePlayer* p)
{
    if (!p)
        return 0;
    if (p->reaction_type == 0 || p->reaction_timer_ms <= 0)
        return 0;
    if (p->reaction_canceled_early)
        return 0;
    float min_frac = 0.0f, max_frac = 0.0f;
    switch (p->reaction_type)
    {
    case 1:
        min_frac = 0.40f;
        max_frac = 0.75f;
        break;
    case 2:
        min_frac = 0.55f;
        max_frac = 0.85f;
        break;
    case 3:
        min_frac = 0.60f;
        max_frac = 0.80f;
        break;
    case 4:
        min_frac = 0.65f;
        max_frac = 0.78f;
        break;
    default:
        return 0;
    }
    if (p->reaction_total_ms <= 0)
        return 0;
    float elapsed = p->reaction_total_ms - p->reaction_timer_ms;
    float frac = elapsed / p->reaction_total_ms;
    if (frac >= min_frac && frac <= max_frac)
    {
        p->reaction_timer_ms = 0;
        p->reaction_type = 0;
        p->reaction_canceled_early = 1;
        return 1;
    }
    return 0;
}
/**
 * @brief Apply directional influence during reaction
 *
 * Accumulates directional input during a reaction, allowing limited player
 * control over reaction movement. The influence is scaled and clamped based
 * on the reaction type's maximum directional influence limit.
 *
 * Influence Mechanics:
 * - Input direction is normalized if magnitude > 1.0
 * - Each input adds 8% of normalized direction to accumulator
 * - Total accumulated influence is clamped to reaction's maximum DI limit
 * - Influence affects reaction movement but doesn't override reaction entirely
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param dx X component of directional input (-1.0 to 1.0)
 * @param dy Y component of directional input (-1.0 to 1.0)
 *
 * @note Only works during active reaction states
 * @note Input is ignored if directional influence is disabled (max <= 0)
 * @note Direction vector is normalized before accumulation
 * @note Accumulated influence is clamped to prevent exceeding limits
 * @note Small incremental additions (8%) allow gradual influence building
 */
void rogue_player_apply_reaction_di(RoguePlayer* p, float dx, float dy)
{
    if (!p)
        return;
    if (p->reaction_type == 0 || p->reaction_timer_ms <= 0)
        return;
    if (p->reaction_di_max <= 0)
        return;
    float mag = sqrtf(dx * dx + dy * dy);
    if (mag > 1.0f && mag > 0)
    {
        dx /= mag;
        dy /= mag;
    }
    p->reaction_di_accum_x += dx * 0.08f;
    p->reaction_di_accum_y += dy * 0.08f;
    float acc_mag = sqrtf(p->reaction_di_accum_x * p->reaction_di_accum_x +
                          p->reaction_di_accum_y * p->reaction_di_accum_y);
    if (acc_mag > p->reaction_di_max && acc_mag > 0)
    {
        float scale = p->reaction_di_max / acc_mag;
        p->reaction_di_accum_x *= scale;
        p->reaction_di_accum_y *= scale;
    }
}
/**
 * @brief Add invincibility frames to the player
 *
 * Grants temporary invincibility by setting or extending the invincibility
 * frame timer. I-frames prevent damage during their duration and are
 * commonly granted after dodges, parries, or other defensive actions.
 *
 * I-frame Management:
 * - Only extends duration if new value is longer than current
 * - Prevents reducing existing i-frame duration
 * - Zero or negative durations are ignored
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param ms Duration in milliseconds to add (must be > 0)
 *
 * @note I-frames are independent of reaction states
 * @note Multiple calls will use the longest duration
 * @note I-frame timer is managed by rogue_player_update_reactions()
 * @note Common sources: dodges (400ms), parries, special abilities
 */
void rogue_player_add_iframes(RoguePlayer* p, float ms)
{
    if (!p)
        return;
    if (ms <= 0)
        return;
    if (p->iframes_ms < ms)
        p->iframes_ms = ms;
}
