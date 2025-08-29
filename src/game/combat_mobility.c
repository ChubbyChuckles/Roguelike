/**
 * @file combat_mobility.c
 * @brief Player mobility and defensive movement mechanics
 *
 * This module implements player mobility systems including:
 * - Dodge roll mechanics with stamina cost and invincibility frames
 * - Aerial attack system for jumping/knockback scenarios
 * - Projectile deflection using parry and riposte states
 *
 * The mobility system provides players with active defensive options
 * that consume stamina and provide temporary invincibility or attack
 * redirection capabilities.
 *
 * Key features:
 * - Dodge roll: High-mobility evasion with stamina cost and iframes
 * - Aerial attacks: Special attacks triggered during airborne state
 * - Projectile deflection: Redirect projectiles using parry/riposte timing
 * - Facing direction integration with mobility actions
 *
 * @note Mobility actions typically consume stamina and have cooldowns
 * @note Dodge roll provides invincibility frames and poise regeneration
 * @note Projectile deflection requires active parry or riposte state
 */

#include "combat.h"
/**
 * @brief Execute a dodge roll in the specified direction
 *
 * Performs a high-mobility evasion maneuver that consumes stamina and provides
 * temporary invincibility frames. The dodge roll is a key defensive mobility
 * option that allows players to avoid attacks while repositioning.
 *
 * Requirements and restrictions:
 * - Player must not be in a reaction state
 * - Cannot be performed during attack strike phase
 * - Requires sufficient stamina (18.0 units)
 *
 * Effects when successful:
 * - Consumes 18 stamina points
 * - Grants 400ms of invincibility frames
 * - Applies 350ms stamina regeneration delay
 * - Restores 10 poise (capped at maximum)
 * - Updates player facing direction
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param pc Pointer to the player's combat state (must not be NULL)
 * @param dir Direction to dodge (0=north, 1=west, 2=east, 3=south)
 * @return 1 if dodge roll was successfully executed, 0 if failed
 *
 * @note Fails if player is in a reaction state or during strike phase
 * @note Stamina cost is fixed at 18.0 units
 * @note Provides 400ms of invincibility frames for evasion
 * @note Includes poise restoration as defensive bonus
 * @note Updates player facing to match dodge direction
 */
int rogue_player_dodge_roll(RoguePlayer* p, RoguePlayerCombat* pc, int dir)
{
    if (!p || !pc)
        return 0;
    if (p->reaction_type != 0)
        return 0;
    if (pc->phase == ROGUE_ATTACK_STRIKE)
        return 0;
    const float COST = 18.0f;
    if (pc->stamina < COST)
        return 0;
    pc->stamina -= COST;
    if (pc->stamina < 0)
        pc->stamina = 0;
    pc->stamina_regen_delay = 350.0f;
    p->iframes_ms = 400.0f;
    if (p->poise + 10.0f < p->poise_max)
        p->poise += 10.0f;
    else
        p->poise = p->poise_max;
    if (dir >= 0 && dir <= 3)
        p->facing = dir;
    return 1;
}
/**
 * @brief Set the player as airborne and prepare for aerial attacks
 *
 * Marks the player as airborne (e.g., from knockback or jumping) and sets
 * a flag indicating that an aerial attack is pending. This enables special
 * combat mechanics for attacks performed while airborne.
 *
 * @param p Pointer to the player entity (currently unused)
 * @param pc Pointer to the player's combat state (must not be NULL)
 *
 * @note Currently a placeholder implementation for aerial combat system
 * @note Sets aerial_attack_pending flag for special attack mechanics
 * @note Player parameter is unused in current implementation
 */
void rogue_player_set_airborne(RoguePlayer* p, RoguePlayerCombat* pc)
{
    (void) p;
    if (!pc)
        return;
    pc->aerial_attack_pending = 1;
}

/**
 * @brief Check if the player is currently airborne
 *
 * Determines whether the player is in an airborne state that would enable
 * special aerial combat mechanics or movement options.
 *
 * @param p Pointer to the player entity (currently unused)
 * @return 1 if player is airborne, 0 otherwise (currently always returns 0)
 *
 * @note Currently returns 0 as airborne state is not implemented
 * @note Placeholder for future aerial movement/combat system
 * @note Player parameter is unused in current implementation
 */
int rogue_player_is_airborne(const RoguePlayer* p)
{
    (void) p;
    return 0;
}
/**
 * @brief Attempt to deflect an incoming projectile using parry/riposte timing
 *
 * Advanced defensive mechanic that allows players to redirect incoming projectiles
 * back toward enemies using precise parry or riposte timing. The deflection
 * reflects the projectile in the direction the player is facing.
 *
 * Requirements for deflection:
 * - Player must have active parry state OR riposte ready state
 * - Both player and combat state pointers must be valid
 *
 * Deflection behavior:
 * - Reflects projectile in player's current facing direction
 * - Ignores original projectile direction (perfect reflection)
 * - Outputs normalized reflection vector
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param pc Pointer to the player's combat state (must not be NULL)
 * @param proj_dir_x X component of incoming projectile direction (currently unused)
 * @param proj_dir_y Y component of incoming projectile direction (currently unused)
 * @param out_reflect_dir_x Output parameter for X component of reflection direction
 * @param out_reflect_dir_y Output parameter for Y component of reflection direction
 * @return 1 if projectile was successfully deflected, 0 if deflection failed
 *
 * @note Requires active parry OR riposte ready state to succeed
 * @note Reflection direction matches player's current facing (cardinal directions)
 * @note Output vectors are normalized (length = 1.0)
 * @note Original projectile direction is ignored for perfect reflection
 * @note Facing directions: 0=north(0,1), 1=west(-1,0), 2=east(1,0), 3=south(0,-1)
 */
int rogue_player_try_deflect_projectile(RoguePlayer* p, RoguePlayerCombat* pc, float proj_dir_x,
                                        float proj_dir_y, float* out_reflect_dir_x,
                                        float* out_reflect_dir_y)
{
    if (!p || !pc)
        return 0;
    int can_deflect = 0;
    if (pc->parry_active)
        can_deflect = 1;
    if (pc->riposte_ready)
        can_deflect = 1;
    if (!can_deflect)
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
    if (out_reflect_dir_x)
        *out_reflect_dir_x = fdx;
    if (out_reflect_dir_y)
        *out_reflect_dir_y = fdy;
    (void) proj_dir_x;
    (void) proj_dir_y;
    return 1;
}
