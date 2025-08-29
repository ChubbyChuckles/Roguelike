/**
 * @file combat_advanced.c
 * @brief Advanced combat mechanics for the roguelike game.
 *
 * This module implements advanced positional and timing-based combat mechanics
 * including backstab attacks, parry/riposte system, and guard break mechanics.
 * These systems provide tactical depth to combat encounters by rewarding
 * precise positioning and timing.
 *
 * Key Features:
 * - Backstab: High damage bonus for attacking from behind
 * - Parry/Riposte: Defensive counter-attack system
 * - Guard Break: Special attack that bypasses defenses
 *
 * @author Game Development Team
 * @date 2025
 */

#include "combat.h"
#include <math.h>

/* Advanced positional & timing bonuses (backstab, parry/riposte, guard break) */
/**
 * @brief Attempts to perform a backstab attack on an enemy.
 *
 * Checks if the player is positioned behind the enemy and within range for a backstab attack.
 * Backstab attacks deal bonus damage when striking from the enemy's rear blind spot.
 * Includes cooldown management to prevent spam attacks.
 *
 * @param p Pointer to the player attempting the backstab
 * @param pc Pointer to the player's combat state
 * @param e Pointer to the target enemy
 * @return 1 if backstab is successful and damage multiplier is set, 0 otherwise
 *
 * @note Requires player to be within 1.5 units of enemy
 * @note Player must be positioned behind enemy (dot product < -0.70)
 * @note Sets 650ms cooldown after successful backstab
 * @note Applies 1.75x damage multiplier for next attack
 */
int rogue_combat_try_backstab(RoguePlayer* p, RoguePlayerCombat* pc, RogueEnemy* e)
{
    if (!p || !pc || !e)
        return 0;
    if (!e->alive)
        return 0;
    if (pc->backstab_cooldown_ms > 0)
        return 0;
    float ex = e->base.pos.x, ey = e->base.pos.y;
    float px = p->base.pos.x, py = p->base.pos.y;
    float dx = px - ex;
    float dy = py - ey;
    float dist2 = dx * dx + dy * dy;
    if (dist2 > 2.25f)
        return 0;
    float fdx = (e->facing == 1) ? -1.0f : (e->facing == 2) ? 1.0f : 0.0f;
    float fdy = (e->facing == 0) ? 1.0f : (e->facing == 3) ? -1.0f : 0.0f;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.0001f)
        return 0;
    dx /= len;
    dy /= len;
    float dot = dx * fdx + dy * fdy;
    if (dot > -0.70f)
        return 0;
    pc->backstab_cooldown_ms = 650.0f;
    pc->backstab_pending_mult = 1.75f;
    return 1;
}
/**
 * @brief Initiates a parry action for the player.
 *
 * Begins the parry state, allowing the player to deflect incoming attacks.
 * Parry provides a defensive window where incoming attacks can be countered.
 * Only one parry can be active at a time.
 *
 * @param p Pointer to the player initiating parry
 * @param pc Pointer to the player's combat state
 *
 * @note Does nothing if parry is already active
 * @note Parry state must be manually reset by successful parry or timeout
 */
void rogue_player_begin_parry(RoguePlayer* p, RoguePlayerCombat* pc)
{
    if (!p || !pc)
        return;
    if (pc->parry_active)
        return;
    pc->parry_active = 1;
    pc->parry_timer_ms = 0.0f;
}
/**
 * @brief Checks if the player currently has an active parry.
 *
 * Returns the current parry state of the player. Used to determine if
 * incoming attacks can be parried and countered.
 *
 * @param pc Pointer to the player's combat state
 * @return 1 if parry is currently active, 0 otherwise
 *
 * @note Safe to call with NULL pointer (returns 0)
 */
int rogue_player_is_parry_active(const RoguePlayerCombat* pc)
{
    return (pc && pc->parry_active) ? 1 : 0;
}
/**
 * @brief Processes an incoming attack during active parry state.
 *
 * Evaluates if an incoming attack can be successfully parried based on the
 * attack direction relative to the player's facing direction. If successful,
 * sets up riposte opportunity and grants invincibility frames.
 *
 * @param p Pointer to the player being attacked
 * @param pc Pointer to the player's combat state
 * @param attack_dir_x X component of the incoming attack direction vector
 * @param attack_dir_y Y component of the incoming attack direction vector
 * @return 1 if attack was successfully parried, 0 otherwise
 *
 * @note Requires parry to be active
 * @note Attack must come from within 80 degrees of player's facing direction
 * @note Successful parry grants 350ms invincibility frames
 * @note Sets up 650ms riposte window with 2.25x damage multiplier
 * @note Deactivates parry state after successful parry
 */
int rogue_player_register_incoming_attack_parry(RoguePlayer* p, RoguePlayerCombat* pc,
                                                float attack_dir_x, float attack_dir_y)
{
    if (!p || !pc)
        return 0;
    if (!pc->parry_active)
        return 0;
    float fdx = 0, fdy = 0;
    switch (p->facing)
    {
    case 0:
        fdy = 1;
        break;
    case 1:
        fdx = -1;
        break;
    case 2:
        fdx = 1;
        break;
    case 3:
        fdy = -1;
        break;
    }
    float alen = sqrtf(attack_dir_x * attack_dir_x + attack_dir_y * attack_dir_y);
    if (alen > 0.0001f)
    {
        attack_dir_x /= alen;
        attack_dir_y /= alen;
    }
    float dot = fdx * attack_dir_x + fdy * attack_dir_y;
    if (dot < 0.10f)
        return 0;
    pc->parry_active = 0;
    pc->riposte_ready = 1;
    pc->riposte_window_ms = 650.0f;
    p->iframes_ms = 350.0f;
    p->riposte_ms = pc->riposte_window_ms;
    return 1;
}
/**
 * @brief Attempts to perform a riposte attack after successful parry.
 *
 * Executes a counter-attack following a successful parry, dealing bonus damage.
 * Riposte can only be performed during the riposte window after a parry.
 * Consumes the riposte opportunity regardless of success.
 *
 * @param p Pointer to the player performing riposte
 * @param pc Pointer to the player's combat state
 * @param e Pointer to the target enemy
 * @return 1 if riposte is successful and damage multiplier is set, 0 otherwise
 *
 * @note Requires riposte_ready to be true
 * @note Target enemy must be alive
 * @note Consumes riposte opportunity after attempt
 * @note Applies 2.25x damage multiplier for next attack
 * @note Resets player's riposte timer
 */
int rogue_player_try_riposte(RoguePlayer* p, RoguePlayerCombat* pc, RogueEnemy* e)
{
    if (!p || !pc || !e)
        return 0;
    if (!pc->riposte_ready)
        return 0;
    if (!e->alive)
        return 0;
    pc->riposte_ready = 0;
    p->riposte_ms = 0.0f;
    pc->riposte_pending_mult = 2.25f;
    return 1;
}
/**
 * @brief Sets up guard break mechanics for the player.
 *
 * Activates guard break state, which allows bypassing enemy defenses and
 * guarantees critical hits. Also enables riposte opportunity as a bonus.
 * Used for special attacks or abilities that break through enemy guards.
 *
 * @param p Pointer to the player (unused parameter)
 * @param pc Pointer to the player's combat state
 *
 * @note Grants 1.5x damage multiplier
 * @note Forces next attack to be critical
 * @note Enables riposte opportunity with 800ms window
 * @note Guard break bonus must be consumed before taking effect
 */
void rogue_player_set_guard_break(RoguePlayer* p, RoguePlayerCombat* pc)
{
    (void) p;
    if (!pc)
        return;
    pc->guard_break_ready = 1;
    pc->riposte_ready = 1;
    pc->riposte_window_ms = 800.0f;
    pc->guard_break_pending_mult = 1.50f;
    pc->force_crit_next_strike = 1;
}
/**
 * @brief Consumes the guard break bonus and applies its effects.
 *
 * Activates the guard break damage multiplier and critical hit guarantee.
 * Should be called when the player actually performs an attack while guard break is ready.
 * Consumes the guard break state to prevent multiple uses.
 *
 * @param pc Pointer to the player's combat state
 * @return 1 if guard break bonus was consumed, 0 if not ready or invalid
 *
 * @note Only works if guard_break_ready is true
 * @note Consumes the guard break state after use
 * @note Damage multiplier and crit guarantee remain active until next attack
 */
int rogue_player_consume_guard_break_bonus(RoguePlayerCombat* pc)
{
    if (!pc)
        return 0;
    if (!pc->guard_break_ready)
        return 0;
    pc->guard_break_ready = 0;
    return 1;
}
/**
 * @brief Gets the current backstab damage multiplier without consuming it.
 *
 * Returns the pending backstab damage multiplier for inspection purposes.
 * Does not modify or consume the backstab state. Used to preview damage
 * calculations before actually applying the backstab bonus.
 *
 * @param pc Pointer to the player's combat state
 * @return Current backstab damage multiplier (1.0 if no backstab pending)
 *
 * @note Returns 1.0 for NULL combat state
 * @note Does not consume or reset the backstab multiplier
 * @note Safe to call multiple times to check multiplier value
 */
float rogue_combat_peek_backstab_mult(const RoguePlayerCombat* pc)
{
    return pc ? pc->backstab_pending_mult : 1.0f;
}
