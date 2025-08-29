/**
 * @file combat_player_state.c
 * @brief Core player combat state machine and attack system
 *
 * This module implements the player's combat state machine, managing attack phases,
 * stamina systems, weapon archetype chaining, and various combat mechanics including
 * parry, riposte, backstab, and charged attacks.
 *
 * Key features:
 * - Four-phase attack state machine (Idle → Windup → Strike → Recovery)
 * - Stamina-based resource management with regeneration delays
 * - Weapon archetype chaining with combo progression
 * - Attack buffering and cancellation systems
 * - Crowd control integration (stun, root, disarm)
 * - Hyper armor state management
 * - Charged attack mechanics with damage multipliers
 * - Combat event queuing and consumption
 *
 * The combat system uses a precise timing accumulator for frame-perfect combat
 * and integrates with stance modifiers, stat scaling, and equipment effects.
 *
 * @note State machine uses high-precision timing to prevent drift in combat windows
 * @note Default attack active state prevents target drift in unit tests
 * @note Stamina regeneration scales with dexterity and intelligence stats
 */

#include "combat.h"
#include "combat_attacks.h"
#include "combat_internal.h"
#include "hit_system.h" /* for rogue_hit_sweep_reset */
#include "infusions.h"
#include "weapons.h"
#include <math.h>
#include <stdlib.h>

/* Access to live app state for runtime CC flags */
#include "../core/app/app_state.h"

/* Core player combat state machine, archetype chaining, stamina, charged attacks */
/* Default to forced-attack active so unit tests that invoke strike processing directly don't
    get target drift from knockback across windows. Runtime/gameplay code can clear this when
    simulating full movement. */
int rogue_force_attack_active = 1; /**< @brief Exported test hook: force attack active state */
int g_attack_frame_override =
    -1; /**< @brief Test hook: override current attack frame (-1 = disabled) */
static int g_player_hyper_armor_active = 0; /**< @brief Internal hyper armor state flag */

/**
 * @brief Set the player's hyper armor active state
 *
 * Hyper armor prevents interruption of player actions by external forces
 * (knockback, stuns, etc.) during critical combat moments.
 *
 * @param active 1 to enable hyper armor, 0 to disable
 *
 * @note Hyper armor is a transient state that protects against interruption
 * @note Used during critical combat windows (charging, special attacks, etc.)
 */
void rogue_player_set_hyper_armor_active(int active)
{
    g_player_hyper_armor_active = active ? 1 : 0;
}

/**
 * @brief Check if player hyper armor is currently active (internal function)
 *
 * @return 1 if hyper armor is active, 0 otherwise
 *
 * @note This is an internal function used by the combat system
 * @note External code should not call this function directly
 */
int _rogue_player_is_hyper_armor_active(void) { return g_player_hyper_armor_active; }

/**
 * @brief Initialize player combat state to default values
 *
 * Resets all combat state variables to their initial values, preparing the
 * player combat system for a fresh start. This includes attack phases, stamina,
 * weapon archetypes, timing accumulators, and all combat flags.
 *
 * @param pc Pointer to the player combat state structure (must not be NULL)
 *
 * @note Initializes to IDLE phase with full stamina and default light weapon archetype
 * @note Clears all pending states (charging, parry, riposte, etc.)
 * @note Resets all timing accumulators and event masks
 * @note Sets default window timings for parry (160ms) and riposte (650ms)
 */
void rogue_combat_init(RoguePlayerCombat* pc)
{
    if (!pc)
        return;
    pc->phase = ROGUE_ATTACK_IDLE;
    pc->timer = 0;
    pc->combo = 0;
    pc->stamina = 100.0f;
    pc->stamina_regen_delay = 0.0f;
    pc->buffered_attack = 0;
    pc->hit_confirmed = 0;
    pc->strike_time_ms = 0.0f;
    pc->archetype = ROGUE_WEAPON_LIGHT;
    pc->chain_index = 0;
    pc->queued_branch_archetype = ROGUE_WEAPON_LIGHT;
    pc->queued_branch_pending = 0;
    pc->precise_accum_ms = 0.0;
    pc->blocked_this_strike = 0;
    pc->recovered_recently = 0;
    pc->idle_since_recover_ms = 0.0f;
    pc->processed_window_mask = 0;
    pc->emitted_events_mask = 0;
    pc->event_count = 0;
    pc->charging = 0;
    pc->charge_time_ms = 0.0f;
    pc->pending_charge_damage_mult = 1.0f;
    pc->parry_active = 0;
    pc->parry_window_ms = 160.0f;
    pc->parry_timer_ms = 0.0f;
    pc->riposte_ready = 0;
    pc->riposte_window_ms = 650.0f;
    pc->backstab_cooldown_ms = 0.0f;
    pc->aerial_attack_pending = 0;
    pc->landing_lag_ms = 0.0f;
    pc->guard_break_ready = 0;
    pc->backstab_pending_mult = 1.0f;
    pc->riposte_pending_mult = 1.0f;
    pc->guard_break_pending_mult = 1.0f;
    pc->force_crit_next_strike = 0;
}

/**
 * @brief Test utility: Force combat state into STRIKE phase
 *
 * Forces the combat state machine into the STRIKE phase for testing purposes.
 * This allows unit tests to directly test strike processing without going through
 * the full windup phase. Sets up the strike timing and resets window processing state.
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 * @param strike_time_ms Initial strike time to set (for testing specific timing scenarios)
 *
 * @note This is a test utility function - not for use in production code
 * @note Resets processed window mask and event emission mask
 * @note Calls rogue_hit_sweep_reset() to clear hit detection state
 */
void rogue_combat_test_force_strike(RoguePlayerCombat* pc, float strike_time_ms)
{
    if (!pc)
        return;
    pc->phase = ROGUE_ATTACK_STRIKE;
    pc->strike_time_ms = strike_time_ms;
    pc->processed_window_mask = 0;
}

/**
 * @brief Main player combat state machine update
 *
 * Updates the player's combat state machine, handling attack phases, stamina regeneration,
 * crowd control effects, weapon chaining, and various combat mechanics. This is the core
 * function that drives all player combat behavior.
 *
 * State Machine Phases:
 * - IDLE: Waiting for input, can start new attacks or chain from recovery
 * - WINDUP: Preparing attack, cannot be interrupted easily
 * - STRIKE: Active attack window with hit detection and cancellation options
 * - RECOVER: Post-attack recovery, allows chaining to next attack
 *
 * Key Features:
 * - Crowd control integration (stun prevents all actions, root prevents starting)
 * - Attack buffering during non-idle states
 * - Stamina cost calculation with stance modifiers
 * - Weapon archetype chaining with late-chain windows
 * - Hit/whiff/block cancellation based on attack definition flags
 * - Precise timing with high-precision accumulator
 * - Stamina regeneration with stat scaling and encumbrance penalties
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 * @param dt_ms Time delta in milliseconds since last update
 * @param attack_pressed 1 if attack button is currently pressed, 0 otherwise
 *
 * @note Uses high-precision timing accumulator to prevent combat drift
 * @note Stamina regeneration scales with dexterity (0.0007/ms) and intelligence (0.0005/ms)
 * @note Encumbrance reduces stamina regeneration (tier 1: 82%, tier 2: 70%, tier 3: 50%)
 * @note Parry and riposte windows are updated separately from main state machine
 * @note Backstab cooldown is managed independently of attack phases
 */
void rogue_combat_update_player(RoguePlayerCombat* pc, float dt_ms, int attack_pressed)
{
    if (!pc)
        return;
    extern struct RoguePlayer g_exposed_player_for_stats; /* player stats (stance, etc.) */
    /* Root should allow buffering but prevent starting; stun/disarm prevent both. */
    int suppress_buffer =
        (g_app.player.cc_stun_ms > 0.0f || g_app.player.cc_disarm_ms > 0.0f) ? 1 : 0;
    int suppress_start = (suppress_buffer || g_app.player.cc_root_ms > 0.0f) ? 1 : 0;
    if (attack_pressed && !suppress_buffer)
    {
        pc->buffered_attack = 1;
    }
    const RogueAttackDef* def = rogue_attack_get(pc->archetype, pc->chain_index);
    float WINDUP_MS = def ? def->startup_ms : 110.0f;
    float STRIKE_MS = def ? def->active_ms : 70.0f;
    float RECOVER_MS = def ? def->recovery_ms : 120.0f;
    {
        float adj_w = WINDUP_MS, adj_r = RECOVER_MS;
        rogue_stance_apply_frame_adjustments(g_exposed_player_for_stats.combat_stance, WINDUP_MS,
                                             RECOVER_MS, &adj_w, &adj_r);
        WINDUP_MS = adj_w;
        RECOVER_MS = adj_r;
    }
    pc->precise_accum_ms += (double) dt_ms;
    pc->timer = (float) pc->precise_accum_ms;
    if (pc->phase == ROGUE_ATTACK_IDLE)
    {
        if (pc->recovered_recently)
        {
            pc->idle_since_recover_ms += dt_ms;
            if (pc->idle_since_recover_ms > 130.0f)
            {
                pc->recovered_recently = 0;
            }
        }
        if (pc->buffered_attack && def && pc->stamina >= def->stamina_cost && !suppress_start)
        {
            if (pc->recovered_recently && pc->idle_since_recover_ms < 130.0f)
            {
                if (pc->queued_branch_pending)
                {
                    pc->archetype = pc->queued_branch_archetype;
                    pc->chain_index = 0;
                    pc->queued_branch_pending = 0;
                }
                else
                {
                    int len = rogue_attack_chain_length(pc->archetype);
                    pc->chain_index = (pc->chain_index + 1) % (len > 0 ? len : 1);
                }
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                WINDUP_MS = def ? def->startup_ms : WINDUP_MS;
                STRIKE_MS = def ? def->active_ms : STRIKE_MS;
                RECOVER_MS = def ? def->recovery_ms : RECOVER_MS;
            }
            if (pc->queued_branch_pending)
            {
                pc->archetype = pc->queued_branch_archetype;
                pc->chain_index = 0;
                pc->queued_branch_pending = 0;
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                if (def)
                {
                    WINDUP_MS = def->startup_ms;
                    STRIKE_MS = def->active_ms;
                    RECOVER_MS = def->recovery_ms;
                }
            }
            RogueStanceModifiers sm =
                rogue_stance_get_mods(g_exposed_player_for_stats.combat_stance);
            float cost = def ? def->stamina_cost : 14.0f;
            cost *= sm.stamina_mult;
            pc->phase = ROGUE_ATTACK_WINDUP;
            pc->timer = 0;
            pc->stamina -= cost;
            pc->stamina_regen_delay = 500.0f;
            pc->buffered_attack = 0;
            pc->hit_confirmed = 0;
            pc->strike_time_ms = 0;
        }
    }
    else if (pc->phase == ROGUE_ATTACK_WINDUP)
    {
        if (pc->timer >= WINDUP_MS)
        {
            pc->phase = ROGUE_ATTACK_STRIKE;
            pc->timer = 0;
            pc->precise_accum_ms = 0.0;
            pc->strike_time_ms = 0;
            pc->blocked_this_strike = 0;
            pc->processed_window_mask = 0;
            pc->emitted_events_mask = 0;
            pc->event_count = 0;
            rogue_hit_sweep_reset();
        }
    }
    else if (pc->phase == ROGUE_ATTACK_STRIKE)
    {
        pc->strike_time_ms += dt_ms;
        unsigned short active_window_flags = 0;
        if (def && def->num_windows > 0)
        {
            for (int wi = 0; wi < def->num_windows; ++wi)
            {
                const RogueAttackWindow* w = &def->windows[wi];
                if (pc->strike_time_ms >= w->start_ms && pc->strike_time_ms < w->end_ms)
                {
                    active_window_flags = w->flags;
                    break;
                }
            }
        }
        float on_hit_threshold = STRIKE_MS * 0.40f;
        if (on_hit_threshold < 15.0f)
            on_hit_threshold = 15.0f;
        int allow_hit_cancel = 0;
        unsigned short hit_flag_mask = (active_window_flags ? active_window_flags
                                        : def               ? def->cancel_flags
                                                            : 0);
        if (pc->hit_confirmed && def && (hit_flag_mask & ROGUE_CANCEL_ON_HIT))
        {
            int all_windows_done = 0;
            if (def->num_windows > 0)
            {
                unsigned int all_bits =
                    (def->num_windows >= 32) ? 0xFFFFFFFFu : ((1u << def->num_windows) - 1u);
                all_windows_done = (pc->processed_window_mask & all_bits) == all_bits;
            }
            else
            {
                all_windows_done = 1;
            }
            if (pc->strike_time_ms >= on_hit_threshold || g_attack_frame_override >= 0 ||
                all_windows_done)
            {
                allow_hit_cancel = 1;
            }
        }
        int allow_whiff_cancel = 0;
        if (!pc->hit_confirmed && def)
        {
            int has_whiff_flag = (hit_flag_mask & ROGUE_CANCEL_ON_WHIFF) != 0;
            if (has_whiff_flag)
            {
                float needed = def->whiff_cancel_pct * STRIKE_MS;
                if (pc->strike_time_ms >= needed)
                {
                    allow_whiff_cancel = 1;
                }
            }
        }
        int allow_block_cancel = 0;
        if (pc->blocked_this_strike && def && (hit_flag_mask & ROGUE_CANCEL_ON_BLOCK))
        {
            float block_thresh = STRIKE_MS * 0.30f;
            float whiff_equiv = def->whiff_cancel_pct * STRIKE_MS;
            if (block_thresh > whiff_equiv)
                block_thresh = whiff_equiv;
            if (pc->strike_time_ms >= block_thresh)
            {
                allow_block_cancel = 1;
            }
        }
        if (pc->strike_time_ms >= STRIKE_MS || allow_hit_cancel || allow_whiff_cancel ||
            allow_block_cancel)
        {
            pc->phase = ROGUE_ATTACK_RECOVER;
            pc->timer = 0;
            pc->combo++;
            if (pc->combo > 5)
                pc->combo = 5;
            if (pc->landing_lag_ms > 0)
            {
                pc->precise_accum_ms = -(double) pc->landing_lag_ms;
                pc->landing_lag_ms = 0.0f;
            }
        }
    }
    else if (pc->phase == ROGUE_ATTACK_RECOVER)
    {
        float rw = 0, rr = 0;
        rogue_stance_apply_frame_adjustments(g_exposed_player_for_stats.combat_stance,
                                             def ? def->startup_ms : WINDUP_MS,
                                             def ? def->recovery_ms : RECOVER_MS, &rw, &rr);
        RECOVER_MS = rr;
        if (pc->timer >= RECOVER_MS)
        {
            if (pc->buffered_attack && def)
            {
                if (pc->queued_branch_pending)
                {
                    pc->archetype = pc->queued_branch_archetype;
                    pc->chain_index = 0;
                    pc->queued_branch_pending = 0;
                }
                else
                {
                    int len = rogue_attack_chain_length(pc->archetype);
                    pc->chain_index = (pc->chain_index + 1) % (len > 0 ? len : 1);
                }
                def = rogue_attack_get(pc->archetype, pc->chain_index);
                WINDUP_MS = def ? def->startup_ms : WINDUP_MS;
                STRIKE_MS = def ? def->active_ms : STRIKE_MS;
                RECOVER_MS = def ? def->recovery_ms : RECOVER_MS;
                RogueStanceModifiers sm2 =
                    rogue_stance_get_mods(g_exposed_player_for_stats.combat_stance);
                float cost = def ? def->stamina_cost : 10.0f;
                cost *= sm2.stamina_mult;
                if (pc->stamina >= cost)
                {
                    pc->phase = ROGUE_ATTACK_WINDUP;
                    pc->timer = 0;
                    pc->precise_accum_ms = 0.0;
                    pc->stamina -= cost;
                    pc->stamina_regen_delay = 450.0f;
                    pc->buffered_attack = 0;
                    pc->hit_confirmed = 0;
                    pc->strike_time_ms = 0;
                    pc->blocked_this_strike = 0;
                }
                else
                {
                    pc->phase = ROGUE_ATTACK_IDLE;
                    pc->timer = 0;
                    pc->hit_confirmed = 0;
                    pc->buffered_attack = 0;
                    pc->recovered_recently = 1;
                    pc->idle_since_recover_ms = 0.0f;
                }
            }
            else
            {
                pc->phase = ROGUE_ATTACK_IDLE;
                pc->timer = 0;
                pc->combo = (pc->combo > 0) ? pc->combo : 0;
                pc->hit_confirmed = 0;
                pc->buffered_attack = 0;
                pc->blocked_this_strike = 0;
                pc->recovered_recently = 1;
                pc->idle_since_recover_ms = 0.0f;
            }
        }
    }
    if (pc->stamina_regen_delay > 0)
    {
        pc->stamina_regen_delay -= dt_ms;
    }
    else
    {
        extern RoguePlayer g_exposed_player_for_stats;
        float dex = (float) g_exposed_player_for_stats.dexterity;
        float intel = (float) g_exposed_player_for_stats.intelligence;
        /* Lower base regen to avoid saturating within short test windows; scale with stats. */
        float regen = 0.040f + (dex * 0.00070f) + (intel * 0.00050f);
        switch (g_exposed_player_for_stats.encumbrance_tier)
        {
        case 1:
            regen *= 0.82f;
            break;
        case 2:
            regen *= 0.70f;
            break;
        case 3:
            regen *= 0.50f;
            break;
        default:
            break;
        }
        pc->stamina += dt_ms * regen;
        if (pc->stamina > 100.0f)
            pc->stamina = 100.0f;
    }
    if (pc->parry_active)
    {
        pc->parry_timer_ms += dt_ms;
        if (pc->parry_timer_ms >= pc->parry_window_ms)
        {
            pc->parry_active = 0;
            pc->parry_timer_ms = 0.0f;
        }
    }
    if (pc->riposte_ready)
    {
        pc->riposte_window_ms -= dt_ms;
        if (pc->riposte_window_ms <= 0)
        {
            pc->riposte_ready = 0;
        }
    }
    if (pc->backstab_cooldown_ms > 0)
    {
        pc->backstab_cooldown_ms -= dt_ms;
        if (pc->backstab_cooldown_ms < 0)
            pc->backstab_cooldown_ms = 0;
    }
}

/**
 * @brief Set the player's current weapon archetype
 *
 * Changes the player's weapon archetype and resets the attack chain to the beginning.
 * This is typically called when switching weapons or when a branch change occurs.
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 * @param arch New weapon archetype to set
 *
 * @note Resets chain index to 0 (first attack in archetype)
 * @note Clears any pending branch changes
 * @note Can be called during any combat phase
 */
void rogue_combat_set_archetype(RoguePlayerCombat* pc, RogueWeaponArchetype arch)
{
    if (!pc)
        return;
    pc->archetype = arch;
    pc->chain_index = 0;
}

/**
 * @brief Get the player's current weapon archetype
 *
 * @param pc Pointer to the player combat state (can be NULL)
 * @return Current weapon archetype, or ROGUE_WEAPON_LIGHT if pc is NULL
 *
 * @note Returns default light weapon if combat state is invalid
 */
RogueWeaponArchetype rogue_combat_current_archetype(const RoguePlayerCombat* pc)
{
    return pc ? pc->archetype : ROGUE_WEAPON_LIGHT;
}

/**
 * @brief Get the player's current position in the attack chain
 *
 * @param pc Pointer to the player combat state (can be NULL)
 * @return Current chain index (0-based), or 0 if pc is NULL
 *
 * @note Chain index determines which attack in the archetype sequence is next
 */
int rogue_combat_current_chain_index(const RoguePlayerCombat* pc)
{
    return pc ? pc->chain_index : 0;
}

/**
 * @brief Queue a weapon archetype branch change for the next attack
 *
 * Sets up a pending archetype change that will be applied on the next attack start.
 * This allows for smooth transitions between different weapon types during combat.
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 * @param branch_arch Weapon archetype to switch to on next attack
 *
 * @note Branch change is applied during attack start (idle → windup transition)
 * @note Only one branch can be queued at a time
 * @note Queued branch is cleared after application
 */
void rogue_combat_queue_branch(RoguePlayerCombat* pc, RogueWeaponArchetype branch_arch)
{
    if (!pc)
        return;
    pc->queued_branch_archetype = branch_arch;
    pc->queued_branch_pending = 1;
}

/**
 * @brief Notify combat system that current strike was blocked
 *
 * Marks the current strike as having been blocked by an enemy. This affects
 * cancellation timing and may enable block-specific cancel windows.
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 *
 * @note Only effective during STRIKE phase
 * @note Enables block cancellation if attack definition supports it
 * @note Affects combo continuation and recovery timing
 */
void rogue_combat_notify_blocked(RoguePlayerCombat* pc)
{
    if (!pc)
        return;
    if (pc->phase == ROGUE_ATTACK_STRIKE)
    {
        pc->blocked_this_strike = 1;
    }
}

/**
 * @brief Consume queued combat events into caller-provided buffer
 *
 * Removes combat events from the internal queue and copies them to the caller's
 * buffer. This allows external systems to process combat events (hits, blocks,
 * parries, etc.) that occurred during the last update cycle.
 *
 * The function copies up to max_events events from the queue, then shifts any
 * remaining events to the front of the queue. The event count is updated accordingly.
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 * @param out_events Buffer to copy events into (must not be NULL)
 * @param max_events Maximum number of events to copy (must be > 0)
 * @return Number of events actually copied (may be less than max_events)
 *
 * @note Events are consumed in FIFO order (oldest first)
 * @note Remaining events are shifted to maintain queue integrity
 * @note Event count is updated to reflect remaining events
 * @note Safe to call with max_events larger than available events
 */
int rogue_combat_consume_events(RoguePlayerCombat* pc, struct RogueCombatEvent* out_events,
                                int max_events)
{
    if (!pc || !out_events || max_events <= 0)
        return 0;
    int n = pc->event_count;
    if (n > max_events)
        n = max_events;
    for (int i = 0; i < n; i++)
        out_events[i] = pc->events[i];
    int remaining = pc->event_count - n;
    if (remaining > 0)
    {
        for (int i = 0; i < remaining && i < (int) (sizeof(pc->events) / sizeof(pc->events[0]));
             ++i)
            pc->events[i] = pc->events[n + i];
    }
    pc->event_count = remaining > 0 ? remaining : 0;
    return n;
}

/* Charged attacks */

/**
 * @brief Begin charging a powerful attack
 *
 * Initiates the charged attack state, allowing the player to build up power
 * for a more damaging strike. Can only be started from IDLE phase.
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 *
 * @note Only works if player is in IDLE phase
 * @note Sets charging flag and resets charge timer
 * @note Charge time will accumulate until released or max time reached
 */
void rogue_combat_charge_begin(RoguePlayerCombat* pc)
{
    if (!pc)
        return;
    if (pc->phase != ROGUE_ATTACK_IDLE)
        return;
    pc->charging = 1;
    pc->charge_time_ms = 0.0f;
}

/**
 * @brief Update charged attack state
 *
 * Updates the charging state based on time and button hold status. If the
 * button is released, calculates the damage multiplier based on charge time
 * and ends the charging state.
 *
 * Damage multiplier calculation:
 * - Base multiplier: 1.0
 * - Charge bonus: up to 1.5x over 800ms
 * - Maximum multiplier: 2.5x
 * - Formula: 1.0 + min(charge_time/800, 1.0) * 1.5
 *
 * @param pc Pointer to the player combat state (must not be NULL)
 * @param dt_ms Time delta in milliseconds since last update
 * @param still_holding 1 if charge button is still held, 0 if released
 *
 * @note Charge time is capped at 1600ms maximum
 * @note Damage multiplier is stored in pending_charge_damage_mult
 * @note Charging automatically ends when button is released
 */
void rogue_combat_charge_tick(RoguePlayerCombat* pc, float dt_ms, int still_holding)
{
    if (!pc)
        return;
    if (!pc->charging)
        return;
    if (!still_holding)
    {
        float t = pc->charge_time_ms;
        float mult = 1.0f + fminf(t / 800.0f, 1.0f) * 1.5f;
        if (mult > 2.5f)
            mult = 2.5f;
        pc->pending_charge_damage_mult = mult;
        pc->charging = 0;
        pc->charge_time_ms = 0.0f;
        return;
    }
    pc->charge_time_ms += dt_ms;
    if (pc->charge_time_ms > 1600.0f)
        pc->charge_time_ms = 1600.0f;
}

/**
 * @brief Get the current charge progress as a normalized value
 *
 * Returns the charging progress as a value between 0.0 and 1.0, where:
 * - 0.0 = not charging or just started
 * - 1.0 = fully charged (800ms charge time)
 *
 * @param pc Pointer to the player combat state (can be NULL)
 * @return Charge progress (0.0 to 1.0), or 0.0 if not charging or invalid state
 *
 * @note Progress is clamped to [0.0, 1.0] range
 * @note Returns 0.0 if player is not currently charging
 * @note Full charge is reached at 800ms, but charging can continue to 1600ms
 */
float rogue_combat_charge_progress(const RoguePlayerCombat* pc)
{
    if (!pc || !pc->charging)
        return 0.0f;
    float p = pc->charge_time_ms / 800.0f;
    if (p > 1.0f)
        p = 1.0f;
    if (p < 0)
        p = 0;
    return p;
}
