/**
 * @file combat_guard.c
 * @brief Player guard and poise system implementation
 *
 * This module implements the player's defensive combat mechanics including:
 * - Guard meter management with drain/recovery mechanics
 * - Perfect guard timing windows and bonuses
 * - Passive blocking system with stat-based chances
 * - Poise damage and reaction system
 * - Damage conversion from physical to elemental types
 * - Reactive shield absorption mechanics
 * - Thorns reflection system
 *
 * The guard system provides active defense through directional blocking, while poise
 * manages the player's ability to maintain composure under attack. Both systems
 * integrate with the stat cache for dynamic scaling based on equipment and abilities.
 *
 * Key features:
 * - Directional guard cone with dot product calculations
 * - Perfect guard windows with meter refunds and poise bonuses
 * - Passive block chance independent of active guarding
 * - Poise-based reaction triggers (stagger, knockdown, heavy hit)
 * - Physical-to-elemental damage conversion
 * - Reactive shield absorption before damage application
 * - Thorns damage reflection with configurable caps
 *
 * @note Phase 7.1: Passive block chance system
 * @note Phase 7.2: Physical to elemental damage conversion
 * @note Phase 7.4: Reactive shield absorption
 * @note Phase 7.5: Thorns reflection system
 */

#include "../core/app/app_state.h"
#include "../core/equipment/equipment_procs.h" /* for reactive shield procs */
#include "combat.h"
#include "combat_internal.h"
#include "stat_cache.h"
#include <math.h>
#include <stdlib.h>

/**
 * @brief Set the player's facing direction for guard mechanics
 *
 * Updates the player's facing direction to the specified cardinal direction.
 * This is used by the guard system to determine the direction the player
 * is actively defending against incoming attacks.
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param dir Facing direction (0=north, 1=west, 2=east, 3=south)
 *
 * @note Direction values outside 0-3 are ignored
 * @note This is an internal helper function for guard mechanics
 */
static void rogue_player_face(RoguePlayer* p, int dir)
{
    if (!p)
        return;
    if (dir < 0 || dir > 3)
        return;
    p->facing = dir;
}

/**
 * @brief Convert player's facing direction to a normalized direction vector
 *
 * Converts the player's cardinal facing direction into a normalized 2D vector
 * for use in guard cone calculations and directional blocking mechanics.
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param dx Output parameter for X component of facing direction (-1=left, 0=neutral, 1=right)
 * @param dy Output parameter for Y component of facing direction (-1=down, 0=neutral, 1=up)
 *
 * @note Vectors are normalized (length = 1.0)
 * @note Default direction is north (0, 1) for invalid facing values
 */
static void rogue_player_facing_dir(const RoguePlayer* p, float* dx, float* dy)
{
    switch (p->facing)
    {
    case 0:
        *dx = 0;
        *dy = 1;
        break;
    case 1:
        *dx = -1;
        *dy = 0;
        break;
    case 2:
        *dx = 1;
        *dy = 0;
        break;
    case 3:
        *dx = 0;
        *dy = -1;
        break;
    default:
        *dx = 0;
        *dy = 1;
        break;
    }
}

/**
 * @brief Initiate active guarding in the specified direction
 *
 * Attempts to start the player guarding in the given direction. Guarding requires
 * available guard meter and will automatically set the player's facing direction.
 * The guard state enables directional blocking against incoming melee attacks.
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param guard_dir Direction to guard (0=north, 1=west, 2=east, 3=south)
 * @return 1 if guarding was successfully initiated, 0 if failed (no guard meter)
 *
 * @note Guarding consumes guard meter over time while active
 * @note Player facing direction is automatically updated to match guard direction
 * @note Guard meter must be > 0 to initiate guarding
 */
int rogue_player_begin_guard(RoguePlayer* p, int guard_dir)
{
    if (!p)
        return 0;
    if (p->guard_meter <= 0.0f)
    {
        p->guarding = 0;
        return 0;
    }
    p->guarding = 1;
    p->guard_active_time_ms = 0.0f;
    rogue_player_face(p, guard_dir);
    return 1;
}
/**
 * @brief Update guard state and meter based on current activity
 *
 * Updates the player's guard meter based on whether they are actively guarding or recovering.
 * When guarding, the meter drains over time with recovery modifiers affecting drain rate.
 * When not guarding, the meter regenerates with recovery modifiers boosting regen rate.
 * Also handles poise regeneration during this update cycle.
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param dt_ms Time delta in milliseconds since last update
 * @return 1 if guard meter was depleted during this update (chip damage occurred), 0 otherwise
 *
 * @note Recovery modifiers from stat cache affect both drain rate and regen rate
 * @note Drain rate is inversely affected by recovery (better recovery = less drain)
 * @note Regen rate is directly boosted by recovery modifiers
 * @note Poise regeneration is also handled during this update
 * @note Guard meter is clamped between 0 and maximum values
 */
int rogue_player_update_guard(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return 0;
    int chip = 0;
    extern struct RogueStatCache g_player_stat_cache;
    float rec_mult = 1.0f + (g_player_stat_cache.guard_recovery_pct / 100.0f);
    if (rec_mult < 0.10f)
        rec_mult = 0.10f; /* prevent negative / zero */
    if (rec_mult > 3.0f)
        rec_mult = 3.0f;
    if (p->guarding)
    {
        p->guard_active_time_ms += dt_ms; /* drain is inversely affected by recovery modifier
                                             (better recovery => less drain) */
        float drain_mult = 1.0f - (g_player_stat_cache.guard_recovery_pct / 150.0f);
        if (drain_mult < 0.25f)
            drain_mult = 0.25f;
        p->guard_meter -= dt_ms * ROGUE_GUARD_METER_DRAIN_HOLD_PER_MS * drain_mult;
        if (p->guard_meter <= 0.0f)
        {
            p->guard_meter = 0.0f;
            p->guarding = 0;
        }
    }
    else
    {
        p->guard_meter += dt_ms * ROGUE_GUARD_METER_RECOVER_PER_MS * rec_mult;
        if (p->guard_meter > p->guard_meter_max)
            p->guard_meter = p->guard_meter_max;
    }
    rogue_player_poise_regen_tick(p, dt_ms);
    return chip;
}

/**
 * @brief Process incoming melee damage through the guard and poise system
 *
 * Comprehensive damage processing function that handles the complete defensive pipeline:
 * 1. God mode bypass
 * 2. I-frame immunity check
 * 3. Passive block chance (Phase 7.1)
 * 4. Active guard blocking with perfect guard mechanics
 * 5. Poise damage and reaction triggers
 * 6. Physical to elemental damage conversion (Phase 7.2)
 * 7. Reactive shield absorption (Phase 7.4)
 * 8. Thorns reflection (Phase 7.5)
 *
 * The function evaluates attack direction against guard cone, applies appropriate
 * damage mitigation, and triggers defensive reactions and procs.
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param raw_damage Raw incoming damage value (physical assumed)
 * @param attack_dir_x X component of attack direction vector
 * @param attack_dir_y Y component of attack direction vector
 * @param poise_damage Amount of poise damage to apply
 * @param out_blocked Output flag set to 1 if attack was blocked (can be NULL)
 * @param out_perfect Output flag set to 1 if attack was perfect blocked (can be NULL)
 * @return Final damage value after all mitigation and absorption effects
 *
 * @note God mode completely bypasses all defensive mechanics
 * @note I-frames provide temporary immunity to all damage
 * @note Guard blocking requires active guarding, sufficient meter, and attack within guard cone
 * @note Perfect guard timing provides meter refund and poise bonus
 * @note Passive blocking provides flat damage reduction independent of guarding
 * @note Poise damage can trigger reactions (stagger, knockdown, heavy hit)
 * @note Damage conversion transforms physical damage to elemental types
 * @note Reactive shields absorb damage before it reaches the player
 * @note Thorns reflect a percentage of final damage back to attacker
 */
int rogue_player_apply_incoming_melee(RoguePlayer* p, float raw_damage, float attack_dir_x,
                                      float attack_dir_y, int poise_damage, int* out_blocked,
                                      int* out_perfect)
{
    if (g_app.god_mode_enabled)
        return 0;
    if (out_blocked)
        *out_blocked = 0;
    if (out_perfect)
        *out_perfect = 0;
    if (!p)
        return (int) raw_damage;
    if (p->iframes_ms > 0.0f)
        return 0;
    if (raw_damage < 0)
        raw_damage = 0;
    float fdx, fdy;
    rogue_player_facing_dir(p, &fdx, &fdy);
    float alen = sqrtf(attack_dir_x * attack_dir_x + attack_dir_y * attack_dir_y);
    if (alen > 0.0001f)
    {
        attack_dir_x /= alen;
        attack_dir_y /= alen;
    }
    float dot = fdx * attack_dir_x + fdy * attack_dir_y;
    int blocked = 0;
    int perfect = 0;
    /* Phase 7.1 passive block chance (independent of guarding) */
    extern struct RogueStatCache g_player_stat_cache; /* use aggregated defensive stats */
    int passive_block = 0;
    if (g_player_stat_cache.block_chance > 0)
    {
        int roll = rand() % 100;
        if (roll < g_player_stat_cache.block_chance)
        {
            passive_block = 1;
        }
    }
    if (p->guarding && p->guard_meter > 0.0f && dot >= ROGUE_GUARD_CONE_DOT)
    {
        blocked = 1;
        perfect = (p->guard_active_time_ms <= p->perfect_guard_window_ms) ? 1 : 0;
        float chip = raw_damage * ROGUE_GUARD_CHIP_PCT;
        if (chip < 1.0f)
            chip = (raw_damage > 0) ? 1.0f : 0.0f;
        if (perfect)
        {
            chip = 0.0f;
            p->guard_meter += ROGUE_PERFECT_GUARD_REFUND;
            if (p->guard_meter > p->guard_meter_max)
                p->guard_meter = p->guard_meter_max;
            p->poise += ROGUE_PERFECT_GUARD_POISE_BONUS;
            if (p->poise > p->poise_max)
                p->poise = p->poise_max;
        }
        else
        {
            p->guard_meter -= ROGUE_GUARD_METER_DRAIN_ON_BLOCK;
            if (p->guard_meter < 0.0f)
                p->guard_meter = 0.0f;
            if (poise_damage > 0)
            {
                float pd = (float) poise_damage * ROGUE_GUARD_BLOCK_POISE_SCALE;
                p->poise -= pd;
                if (p->poise < 0.0f)
                    p->poise = 0.0f;
                p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT;
            }
        }
        rogue_procs_event_block(); /* trigger potential reactive shield proc */
        /* Apply reactive shield absorb to post-guard chip damage before returning. */
        if (chip > 0.0f)
        {
            int absorb_pool = rogue_procs_absorb_pool();
            if (absorb_pool > 0)
            {
                int consumed = rogue_procs_consume_absorb((int) chip);
                chip -= (float) consumed;
                if (chip < 0.0f)
                    chip = 0.0f;
            }
        }
        if (out_blocked)
            *out_blocked = 1;
        if (perfect && out_perfect)
            *out_perfect = 1;
        return (int) chip;
    }
    if (passive_block)
    {
        blocked = 1; /* Flat reduction by block_value; never less than 0 */
        int red = g_player_stat_cache.block_value;
        if (red < 0)
            red = 0;
        raw_damage -= (float) red;
        if (raw_damage < 0)
            raw_damage = 0;
        rogue_procs_event_block(); /* passive block also triggers block procs */
        /* Apply reactive shield absorb to post-block damage before returning. */
        if (raw_damage > 0)
        {
            int absorb_pool = rogue_procs_absorb_pool();
            if (absorb_pool > 0)
            {
                int consumed = rogue_procs_consume_absorb((int) raw_damage);
                raw_damage -= (float) consumed;
                if (raw_damage < 0)
                    raw_damage = 0;
            }
        }
        if (out_blocked)
            *out_blocked = 1;
        return (int) raw_damage;
    }
    int triggered_reaction = 0;
    if (poise_damage > 0 && !_rogue_player_is_hyper_armor_active())
    {
        float before = p->poise;
        p->poise -= (float) poise_damage;
        if (p->poise < 0.0f)
            p->poise = 0.0f;
        if (before > 0.0f && p->poise <= 0.0f)
        {
            rogue_player_apply_reaction(p, 2);
            triggered_reaction = 1;
        }
    }
    if (!triggered_reaction)
    {
        if (raw_damage >= 80)
        {
            rogue_player_apply_reaction(p, 3);
        }
        else if (raw_damage >= 25)
        {
            rogue_player_apply_reaction(p, 1);
        }
    }
    p->poise_regen_delay_ms = ROGUE_POISE_REGEN_DELAY_AFTER_HIT;
    /* Phase 7.2 damage conversion (physical -> elemental) implementation.
        Since this function currently receives only a single raw_damage assumed physical, we
       internally partition it. Ordering: passive block -> guard block -> conversion -> (future
       elemental mitigation when player resist fields added). We expose converted components via
       local variables for potential future telemetry (unused now). */
    float remain_phys = raw_damage;
    if (remain_phys < 0)
        remain_phys = 0;
    int c_fire = g_player_stat_cache.phys_conv_fire_pct;
    if (c_fire < 0)
        c_fire = 0;
    int c_frost = g_player_stat_cache.phys_conv_frost_pct;
    if (c_frost < 0)
        c_frost = 0;
    int c_arc = g_player_stat_cache.phys_conv_arcane_pct;
    if (c_arc < 0)
        c_arc = 0;
    int total_conv = c_fire + c_frost + c_arc;
    if (total_conv > 95)
        total_conv = 95; /* retain at least 5% physical identity */
    float fire_amt = 0, frost_amt = 0, arc_amt = 0;
    if (total_conv > 0 && remain_phys > 0)
    {
        fire_amt = remain_phys * ((float) c_fire / 100.0f);
        frost_amt = remain_phys * ((float) c_frost / 100.0f);
        arc_amt = remain_phys * ((float) c_arc / 100.0f);
        float sum = fire_amt + frost_amt + arc_amt;
        if (sum > remain_phys)
        {
            float scale = remain_phys / sum;
            fire_amt *= scale;
            frost_amt *= scale;
            arc_amt *= scale;
        }
        remain_phys -= (fire_amt + frost_amt + arc_amt);
    }
    /* (Future) Apply elemental resistances once player stat cache exposes them (currently only
     * enemy has). */
    raw_damage = remain_phys + fire_amt + frost_amt + arc_amt; /* conservation */
    /* Phase 7.4 reactive shield absorb: consume before reflect. */
    int absorb_pool = rogue_procs_absorb_pool();
    if (absorb_pool > 0 && raw_damage > 0)
    {
        int consumed = rogue_procs_consume_absorb((int) raw_damage);
        raw_damage -= (float) consumed;
        if (raw_damage < 0)
            raw_damage = 0;
    }
    /* Phase 7.5 thorns reflect: reflect percent of final post-conversion damage; cap applies. */
    if (g_player_stat_cache.thorns_percent > 0 && raw_damage > 0)
    {
        int reflect = (int) ((raw_damage * g_player_stat_cache.thorns_percent) / 100.0f);
        if (g_player_stat_cache.thorns_cap > 0 && reflect > g_player_stat_cache.thorns_cap)
            reflect = g_player_stat_cache.thorns_cap; /* TODO: hook into enemy damage pipeline when
                                                         attacker context available */
        (void) reflect;
    }
    return (int) raw_damage;
}

/**
 * @brief Handle poise regeneration over time
 *
 * Manages the player's poise recovery system with delay mechanics and non-linear
 * regeneration rates. Poise regeneration is delayed after taking damage and
 * accelerates as poise gets lower (missing poise ratio squared scaling).
 *
 * @param p Pointer to the player entity (must not be NULL)
 * @param dt_ms Time delta in milliseconds since last update
 *
 * @note Poise regeneration is delayed after taking damage (ROGUE_POISE_REGEN_DELAY_AFTER_HIT)
 * @note Regeneration rate increases quadratically as poise gets lower
 * @note Base regeneration is ROGUE_POISE_REGEN_BASE_PER_MS
 * @note Poise is clamped between 0 and maximum values
 */
void rogue_player_poise_regen_tick(RoguePlayer* p, float dt_ms)
{
    if (!p)
        return;
    if (p->poise_regen_delay_ms > 0)
    {
        p->poise_regen_delay_ms -= dt_ms;
        if (p->poise_regen_delay_ms < 0)
            p->poise_regen_delay_ms = 0;
    }
    if (p->poise_regen_delay_ms <= 0 && p->poise < p->poise_max)
    {
        float missing = p->poise_max - p->poise;
        float ratio = missing / p->poise_max;
        if (ratio < 0)
            ratio = 0;
        if (ratio > 1)
            ratio = 1;
        float regen = (ROGUE_POISE_REGEN_BASE_PER_MS * dt_ms) * (1.0f + 1.75f * ratio * ratio);
        p->poise += regen;
        if (p->poise > p->poise_max)
            p->poise = p->poise_max;
    }
}
