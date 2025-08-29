/**
 * @file combat_strike.c
 * @brief Core player strike processing and damage application system
 *
 * This module implements the comprehensive strike processing system that handles:
 * - Attack window management and timing
 * - Multi-component damage calculation (physical, fire, frost, arcane)
 * - Armor penetration and mitigation with override capabilities
 * - Critical hit system with pre/post-mitigation layering modes
 * - Obstruction checking and damage attenuation
 * - Hit feedback, particles, and sound effects
 * - Enemy staggering, knockback, and death processing
 * - Damage event emission for tracking and analytics
 * - Team filtering and friendly fire prevention
 *
 * Key features:
 * - Window-based attack processing with precise timing
 * - Complex damage formula incorporating stats, weapons, infusions, and stance
 * - Armor penetration mechanics (flat and percentage-based)
 * - Critical hit system with configurable layering (pre/post-mitigation)
 * - Obstruction system with line-of-sight checking
 * - Comprehensive hit feedback system (visual, audio, particles)
 * - Poise damage and enemy staggering mechanics
 * - Execution detection based on health percentage and overkill
 *
 * The strike system processes attacks in phases:
 * 1. Window activation and event emission
 * 2. Target acquisition via weapon sweep
 * 3. Damage calculation with all modifiers
 * 4. Mitigation application with penetration
 * 5. Hit application and feedback
 * 6. Enemy state updates (health, poise, stagger)
 *
 * @note Phase 7.1: Hit candidates sourced exclusively from sweep
 * @note Phase 7.5: Hitstop system for first strike enemy
 * @note Phase 4: Full feedback system with refined knockback and effects
 */

#include "../core/progression/progression_ratings.h"
#include "buffs.h"
#include "combat.h"
#include "combat_attacks.h"
#include "combat_internal.h"
#include "hit_feedback.h"
#include "hit_system.h" /* Phase 1-2 geometry (currently unused gating placeholder) */
#include "infusions.h"
#include "lock_on.h"
#include "navigation.h"
#include "weapons.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* External declarations and test hooks */
extern int
    g_attack_frame_override; /**< @brief Test hook: override current attack frame (-1 = disabled) */
extern int rogue_force_attack_active; /**< @brief Test hook: force attack active state */
extern int g_crit_layering_mode;      /**< @brief Critical hit layering mode: 0=pre-mitigation,
                                         1=post-mitigation */
extern void rogue_app_add_hitstop(float ms); /**< @brief Hitstop API for impact feedback */

/** @brief Runtime test hook for strict team filtering (default off) */
static int g_strict_team_filter = 0;

/**
 * @brief Set strict team filtering mode for testing
 *
 * Controls team-based friendly fire filtering behavior:
 * - Strict mode (enabled): Skip same-team enemies, including team_id==0
 * - Default mode (disabled): Treat team_id==0 as neutral, skip only matching non-zero team IDs
 *
 * @param enable 1 to enable strict filtering, 0 for default behavior
 *
 * @note This is primarily a test hook for controlling friendly fire behavior
 * @note Default mode allows team_id==0 (neutral) to be hittable by any team
 */
/**
 * @brief Configure team filtering behavior for friendly fire prevention
 *
 * Controls how team IDs are used to prevent friendly fire during combat.
 * Two modes are supported:
 * - Strict mode: Skip any enemy with matching team_id (including team_id==0)
 * - Default mode: Treat team_id==0 as neutral, skip only when both IDs are non-zero and equal
 *
 * @param enable 1 to enable strict team filtering, 0 for default behavior
 *
 * @note Used primarily by unit tests to ensure deterministic behavior
 * @note Default mode allows neutral entities (team_id==0) to be attacked by anyone
 * @note Strict mode prevents all same-team attacks including neutral vs neutral
 */
void rogue_combat_set_strict_team_filter(int enable) { g_strict_team_filter = enable ? 1 : 0; }

/** @brief Helper function to check if navigation tile is blocked */
static int combat_nav_is_blocked(int tx, int ty) { return rogue_nav_is_blocked(tx, ty); }

/* External UI hooks */
/** @brief External UI hook for adding damage numbers */
void rogue_add_damage_number(float x, float y, int amount, int from_player);
/** @brief External UI hook for adding damage numbers with critical hit indication */
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit);

/* Local helper mirrors mitigation logic but allows overriding armor value explicitly.
   This avoids side effects of temporarily mutating enemy armor when applying penetration. */
/**
 * @brief Local integer clamping utility
 *
 * @param v Value to clamp
 * @param lo Minimum value
 * @param hi Maximum value
 * @return Clamped value within [lo, hi] range
 */
static int _clampi(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

/**
 * @brief Local effective physical resistance calculation
 *
 * Mirrors the main mitigation system's physical resistance curve calculation
 * with diminishing returns. Used locally to avoid side effects when applying
 * armor penetration.
 *
 * @param p Raw physical resistance percentage (0-90)
 * @return Effective resistance percentage (0-75)
 *
 * @note Uses same piecewise curve as main mitigation system
 * @note Linear scaling 0-50%, diminishing returns 50-90% approaching 70%
 * @note Maximum effective resistance capped at 75%
 */
static int _effective_phys_resist_local(int p)
{
    if (p <= 0)
        return 0;
    if (p > 90)
        p = 90;
    float pf = (float) p;
    float efff = 0.0f;
    if (pf <= 50.0f)
    {
        efff = pf;
    }
    else
    {
        float over = pf - 50.0f;
        efff = 50.0f + over * 0.50f;
    }
    if (efff < 0.0f)
        efff = 0.0f;
    if (efff > 75.0f)
        efff = 75.0f;
    return (int) floorf(efff + 0.5f);
}
/**
 * @brief Apply damage mitigation with explicit armor override
 *
 * Local mirror of the main mitigation logic that allows overriding the enemy's
 * armor value explicitly. This enables armor penetration mechanics without
 * permanently modifying the enemy's armor stat.
 *
 * Mirrors mitigation pipeline:
 * 1. True damage bypasses all mitigation
 * 2. Physical damage: armor reduction + resistance curve + softcap
 * 3. Elemental damage: simple percentage resistance reduction
 * 4. Minimum damage floor (1 damage minimum)
 * 5. Overkill calculation for damage exceeding health
 *
 * @param e Pointer to enemy entity (must not be NULL, must be alive)
 * @param raw Raw incoming damage value
 * @param dmg_type Damage type (ROGUE_DMG_PHYSICAL, ROGUE_DMG_FIRE, etc.)
 * @param override_armor Armor value to use instead of enemy's actual armor
 * @param out_overkill Output parameter for overkill damage (can be NULL)
 * @return Final mitigated damage value (minimum 1)
 *
 * @note Used for armor penetration mechanics without side effects
 * @note Mirrors main mitigation system but with armor override capability
 * @note True damage completely bypasses armor and resistance
 * @note Physical damage uses concave resistance curve with diminishing returns
 * @note Softcap prevents excessive mitigation on high-damage attacks
 * @note Overkill is damage that exceeds remaining health
 */
static int _rogue_apply_mitig_with_override_armor(const RogueEnemy* e, int raw,
                                                  unsigned char dmg_type, int override_armor,
                                                  int* out_overkill)
{
    if (!e || !e->alive)
        return 0;
    int dmg = raw;
    if (dmg < 0)
        dmg = 0;
    if (dmg_type != ROGUE_DMG_TRUE)
    {
        if (dmg_type == ROGUE_DMG_PHYSICAL)
        {
            int armor = override_armor;
            if (armor > 0)
            {
                if (armor >= dmg)
                    dmg = (dmg > 1 ? 1 : dmg);
                else
                    dmg -= armor;
            }
            int pr_raw = _clampi(e->resist_physical, 0, 90);
            int pr = _effective_phys_resist_local(pr_raw);
            if (pr > 0)
            {
                int reduce = (dmg * pr) / 100;
                dmg -= reduce;
            }
            if (raw >= ROGUE_DEF_SOFTCAP_MIN_RAW)
            {
                float armor_frac = 0.0f;
                if (override_armor > 0)
                {
                    armor_frac = (float) override_armor / (float) (raw + override_armor);
                    if (armor_frac > 0.90f)
                        armor_frac = 0.90f;
                }
                float total_frac = armor_frac + (float) pr / 100.0f;
                if (total_frac > 0.0f)
                {
                    if (total_frac > ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD)
                    {
                        float excess = total_frac - ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD;
                        float adjusted_excess = excess * ROGUE_DEF_SOFTCAP_SLOPE;
                        float capped_total =
                            ROGUE_DEF_SOFTCAP_REDUCTION_THRESHOLD + adjusted_excess;
                        if (capped_total > ROGUE_DEF_SOFTCAP_MAX_REDUCTION)
                            capped_total = ROGUE_DEF_SOFTCAP_MAX_REDUCTION;
                        int target = (int) floorf((float) raw * (1.0f - capped_total) + 0.5f);
                        if (target < 1)
                            target = 1;
                        if (target > dmg)
                        { /* keep lower dmg (more mitigation) */
                        }
                        else
                            dmg = target;
                        int floor_min = (int) floorf((float) raw * 0.05f + 0.5f);
                        if (dmg < floor_min)
                            dmg = floor_min;
                    }
                }
            }
        }
        else
        {
            int resist = 0;
            switch (dmg_type)
            {
            case ROGUE_DMG_FIRE:
                resist = e->resist_fire;
                break;
            case ROGUE_DMG_FROST:
                resist = e->resist_frost;
                break;
            case ROGUE_DMG_ARCANE:
                resist = e->resist_arcane;
                break;
            default:
                break;
            }
            resist = _clampi(resist, 0, 90);
            if (resist > 0)
            {
                int reduce = (dmg * resist) / 100;
                dmg -= reduce;
            }
        }
    }
    if (dmg < 1)
        dmg = 1;
    int overkill = 0;
    if (e->health - dmg < 0)
    {
        overkill = dmg - e->health;
    }
    if (out_overkill)
        *out_overkill = overkill;
    return dmg;
}

/**
 * @brief Process player strike against enemies in range
 *
 * Comprehensive strike processing function that handles the complete attack pipeline:
 * window management, target acquisition, damage calculation, mitigation, and feedback.
 * This is the core function that executes player attacks against enemy targets.
 *
 * Processing Pipeline:
 * 1. Window Activation: Determine active attack windows and emit events
 * 2. Target Acquisition: Get enemy list from weapon sweep (Phase 7.1)
 * 3. Team Filtering: Apply friendly fire rules based on team IDs
 * 4. Damage Calculation: Complex formula with stats, weapons, infusions, stance
 * 5. Armor Penetration: Apply flat and percentage armor reduction
 * 6. Obstruction Check: Line-of-sight verification with damage attenuation
 * 7. Critical Hit: Chance calculation with layering mode support
 * 8. Mitigation: Apply enemy defenses with penetration effects
 * 9. Hit Application: Deal damage and update enemy state
 * 10. Feedback: Visual effects, sound, particles, knockback
 * 11. Event Emission: Record damage events for tracking
 *
 * Key Features:
 * - Multi-component damage (physical, fire, frost, arcane)
 * - Armor penetration mechanics (flat + percentage)
 * - Critical hit system with pre/post-mitigation layering
 * - Obstruction system with 55% damage attenuation
 * - Comprehensive hit feedback (numbers, particles, sound, hitstop)
 * - Poise damage and enemy staggering
 * - Execution detection based on health/overkill percentages
 * - Weapon durability and familiarity tracking
 *
 * @param pc Pointer to player combat state (must not be NULL)
 * @param player Pointer to player entity (must not be NULL)
 * @param enemies Array of enemy entities to check for hits
 * @param enemy_count Number of enemies in the array
 * @return Number of enemies killed by this strike
 *
 * @note Only processes strikes when combat state is in STRIKE phase
 * @note Phase 7.1: Hit candidates sourced exclusively from sweep
 * @note Phase 7.5: Hitstop applied to first enemy hit
 * @note Phase 4: Full feedback system with refined effects
 * @note Aerial attacks get 20% damage bonus
 * @note Backstab, riposte, and guard break apply damage multipliers
 * @note Obstructed hits take 55% damage penalty
 * @note Critical hits respect layering mode (pre/post-mitigation)
 * @note Execution triggered by low health percentage or high overkill
 */
int rogue_combat_player_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy enemies[],
                               int enemy_count)
{
    if (pc->phase != ROGUE_ATTACK_STRIKE)
        return 0;

    /* Phase 1: Initial Setup and Window State Management */
    /* Reset per-strike hit mask at the start of a strike when entering with a fresh window state.
       This mirrors the reset done during the state machine transition, but unit tests call this
       function directly and set masks to zero manually. Do this before any window activation code
       mutates emitted/processed masks. */
    if (pc->processed_window_mask == 0 && pc->emitted_events_mask == 0)
    {
        rogue_hit_sweep_reset();
    }
    if (pc->strike_time_ms <= 0.0f && pc->processed_window_mask != 0)
    {
        pc->processed_window_mask = 0;
        pc->emitted_events_mask = 0;
        pc->event_count = 0;
    }
    int kills = 0;

    /* Phase 2: Window Activation and Event Emission */
    /* Legacy reach/arc fully removed: hit candidates sourced exclusively from sweep (Phase 7.1) */
    float px = player->base.pos.x;
    float py = player->base.pos.y;
    float cx = px;
    float cy = py;
    unsigned int newly_active_mask = 0;
    pc->current_window_flags = 0;
    const RogueAttackDef* def = rogue_attack_get(pc->archetype, pc->chain_index);
    if (def && def->num_windows > 0)
    {
        for (int wi = 0; wi < def->num_windows && wi < 32; ++wi)
        {
            const RogueAttackWindow* w = &def->windows[wi];
            int active = (pc->strike_time_ms >= w->start_ms && pc->strike_time_ms < w->end_ms);
            if (active)
            {
                newly_active_mask |= (1u << wi);
                /* Only expose defensive window flags (e.g., hyper armor) to the UI/state. */
                pc->current_window_flags = (unsigned short) (w->flags & ROGUE_WINDOW_HYPER_ARMOR);
                if (!(pc->emitted_events_mask & (1u << wi)))
                {
                    if (pc->event_count < (int) (sizeof(pc->events) / sizeof(pc->events[0])))
                    {
                        pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_BEGIN_WINDOW;
                        pc->events[pc->event_count].data = (unsigned short) wi;
                        pc->events[pc->event_count].t_ms = pc->strike_time_ms;
                        pc->event_count++;
                    }
                    pc->emitted_events_mask |= (1u << wi);
                }
            }
            else if (!active && (pc->emitted_events_mask & (1u << wi)) &&
                     !(pc->processed_window_mask & (1u << wi)))
            {
                if (pc->event_count < (int) (sizeof(pc->events) / sizeof(pc->events[0])))
                {
                    pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_END_WINDOW;
                    pc->events[pc->event_count].data = (unsigned short) wi;
                    pc->events[pc->event_count].t_ms = pc->strike_time_ms;
                    pc->event_count++;
                }
                /* Do not mark as processed unless damage for this window was actually applied.
                    The processed_window_mask is reserved for windows that have dealt damage to
                    prevent re-application. Natural end without processing should not block a
                    later entry into the window (e.g., tests that jump time to window start). */
            }
        }
    }
    else if (def)
    {
        newly_active_mask = 1u;
        if (pc->strike_time_ms >= def->active_ms)
            newly_active_mask = 0;
    }
    unsigned int process_mask = newly_active_mask & ~pc->processed_window_mask;
    if (process_mask == 0)
        return 0;

    /* Phase 3: Process Active Windows */
    for (int wi = 0; wi < 32; ++wi)
    {
        if (!(process_mask & (1u << wi)))
            continue;
        /* Reset per-window hit mask so overlapping windows can re-hit the same target once. */
        rogue_hit_sweep_reset();
        float window_mult = 1.0f;
        float bleed_build = 0.0f, frost_build = 0.0f;
        if (def && wi < def->num_windows)
        {
            const RogueAttackWindow* w = &def->windows[wi];
            if (w->damage_mult > 0.0f)
                window_mult = w->damage_mult;
            bleed_build = w->bleed_build;
            frost_build = w->frost_build;
            if (w->flags & ROGUE_WINDOW_HYPER_ARMOR)
            {
                rogue_player_set_hyper_armor_active(1);
            }
        }

        /* Phase 4: Target Acquisition from Weapon Sweep */
        /* Phase 7 integrated: sweep apply returns definitive enemy list */
        const int* sweep_indices = NULL;
        int sweep_count = rogue_combat_weapon_sweep_apply(pc, player, enemies, enemy_count);
        rogue_hit_last_indices(&sweep_indices);
        int first_strike_enemy_processed = 0; /* for hitstop (Phase 7.5) */

        /* Phase 5: Process Each Hit Target */
        for (int si = 0; si < sweep_count; ++si)
        {
            int i = sweep_indices[si];
            if (i < 0 || i >= enemy_count)
                continue;
            if (!enemies[i].alive)
                continue;

            /* Team filtering: prevent friendly fire based on team IDs */
            if (g_strict_team_filter)
            {
                if (enemies[i].team_id == player->team_id)
                    continue;
            }
            else
            {
                if (enemies[i].team_id != 0 && player->team_id != 0 &&
                    enemies[i].team_id == player->team_id)
                    continue;
            }

            float ex = enemies[i].base.pos.x;
            float ey = enemies[i].base.pos.y;

            /* Phase 6: Base Damage Calculation */
            int effective_strength = player->strength + rogue_buffs_get_total(0);
            int base = 1 + effective_strength / 5;
            float scaled = (float) base;
            if (def)
            {
                scaled = def->base_damage + (float) effective_strength * def->str_scale +
                         (float) player->dexterity * def->dex_scale +
                         (float) player->intelligence * def->int_scale;
                if (scaled < 1.0f)
                    scaled = 1.0f;
            }

            /* Combo scaling with hard cap */
            float combo_scale = 1.0f + (pc->combo * 0.08f);
            if (combo_scale > 1.4f)
                combo_scale = 1.4f;

            /* Weapon and equipment modifiers */
            const RogueWeaponDef* wdef = rogue_weapon_get(player->equipped_weapon_id);
            RogueStanceModifiers sm = rogue_stance_get_mods(player->combat_stance);
            const RogueInfusionDef* inf = rogue_infusion_get(player->weapon_infusion);
            if (wdef)
            {
                scaled += wdef->base_damage;
                scaled += (float) player->strength * wdef->str_scale +
                          (float) player->dexterity * wdef->dex_scale +
                          (float) player->intelligence * wdef->int_scale;
            }

            /* Weapon familiarity and durability modifiers */
            float fam_bonus = rogue_weapon_get_familiarity_bonus(player->equipped_weapon_id);
            float durability_mult = 1.0f;
            if (wdef)
            {
                float cur = rogue_weapon_current_durability(wdef->id);
                if (cur > 0)
                {
                    float pct = cur / (wdef->durability_max > 0 ? wdef->durability_max : 1.0f);
                    if (pct < 0.5f)
                    {
                        float frac = pct / 0.5f;
                        durability_mult = 0.70f + 0.30f * frac;
                    }
                }
            }

            /* Phase 7: Composite Damage Calculation */
            float base_composite = scaled * combo_scale * window_mult * sm.damage_mult *
                                   (1.0f + fam_bonus) * durability_mult;
            float comp_phys = base_composite;
            float comp_fire = 0.0f, comp_frost = 0.0f, comp_arc = 0.0f;
            if (inf)
            {
                comp_fire = base_composite * inf->fire_add;
                comp_frost = base_composite * inf->frost_add;
                comp_arc = base_composite * inf->arcane_add;
                comp_phys = base_composite * inf->phys_scalar;
            }

            /* Special attack multipliers */
            float raw = comp_phys + comp_fire + comp_frost + comp_arc;
            if (pc->aerial_attack_pending)
            {
                raw *= 1.20f;
                pc->aerial_attack_pending = 0;
                pc->landing_lag_ms += 120.0f;
            }
            if (pc->backstab_pending_mult > 1.0f)
            {
                raw *= pc->backstab_pending_mult;
                pc->backstab_pending_mult = 1.0f;
            }
            if (pc->riposte_pending_mult > 1.0f)
            {
                raw *= pc->riposte_pending_mult;
                pc->riposte_pending_mult = 1.0f;
            }
            if (pc->guard_break_pending_mult > 1.0f)
            {
                raw *= pc->guard_break_pending_mult;
                pc->guard_break_pending_mult = 1.0f;
            }
            if (pc->pending_charge_damage_mult > 1.0f)
            {
                raw *= pc->pending_charge_damage_mult;
            }

            /* Component damage partitioning */
            float t_parts = comp_phys + comp_fire + comp_frost + comp_arc;
            if (t_parts < 0.0001f)
                t_parts = 1.0f;
            float part_phys = raw * (comp_phys / t_parts);
            float part_fire = raw * (comp_fire / t_parts);
            float part_frost = raw * (comp_frost / t_parts);
            float part_arc = raw * (comp_arc / t_parts);

            /* Combo minimum damage floor */
            int dmg = (int) floorf(raw + 0.5f);
            if (pc->combo > 0)
            {
                int min_noncrit = (int) floorf(scaled + pc->combo + 0.5f);
                int hard_cap = (int) floorf(scaled * 1.4f + 0.5f);
                if (min_noncrit > hard_cap)
                    min_noncrit = hard_cap;
                if (dmg < min_noncrit)
                    dmg = min_noncrit;
            }

            /* Phase 8: Obstruction Check */
            int obstructed = 0;
            int override_used = 0;
            int ov = _rogue_combat_call_obstruction_test(px, py, ex, ey);
            if (ov == 0 || ov == 1)
            {
                obstructed = ov;
                override_used = 1;
            }
            if (!override_used)
            {
                /* Line-of-sight check with tile-based obstruction */
                float rx0 = cx;
                float ry0 = cy;
                float rx1 = ex;
                float ry1 = ey;
                int tx0 = (int) floorf(rx0);
                int ty0 = (int) floorf(ry0);
                int tx1 = (int) floorf(rx1);
                int ty1 = (int) floorf(ry1);
                int steps = (abs(tx1 - tx0) > abs(ty1 - ty0) ? abs(tx1 - tx0) : abs(ty1 - ty0));
                if (steps < 1)
                    steps = 1;
                float fx = (float) (tx1 - tx0) / (float) steps;
                float fy = (float) (ty1 - ty0) / (float) steps;
                float sx = (float) tx0 + 0.5f;
                float sy = (float) ty0 + 0.5f;
                for (int step_i = 0; step_i <= steps; ++step_i)
                {
                    int cx_t = (int) floorf(sx);
                    int cy_t = (int) floorf(sy);
                    if (!(cx_t == tx0 && cy_t == ty0) && !(cx_t == tx1 && cy_t == ty1))
                    {
                        if (combat_nav_is_blocked(cx_t, cy_t))
                        {
                            obstructed = 1;
                            break;
                        }
                    }
                    sx += fx;
                    sy += fy;
                }
            }
            if (obstructed)
            {
                /* Apply 55% damage attenuation for obstructed hits */
                float atten = 0.55f;
                part_phys *= atten;
                part_fire *= atten;
                part_frost *= atten;
                part_arc *= atten;
                raw *= atten;
                dmg = (int) floorf(raw + 0.5f);
                if (dmg < 1)
                    dmg = 1;
            }

            /* Phase 9: Critical Hit Calculation */
            float raw_total = (float) dmg;
            float dex_bonus = player->dexterity * 0.0035f;
            if (dex_bonus > 0.55f)
                dex_bonus = 0.55f;
            float crit_from_rating =
                rogue_rating_effective_percent(ROGUE_RATING_CRIT, player->crit_rating) * 0.01f;
            float crit_chance =
                0.05f + dex_bonus + (float) player->crit_chance * 0.01f + crit_from_rating;
            if (crit_chance > 0.80f)
                crit_chance = 0.80f;
            int is_crit = 0;
            if (pc->force_crit_next_strike)
            {
                is_crit = 1;
                pc->force_crit_next_strike = 0;
            }
            else
            {
                extern int g_force_crit_mode;
                if (g_force_crit_mode >= 0)
                {
                    is_crit = g_force_crit_mode ? 1 : 0;
                }
                else
                {
                    is_crit = (((float) rand() / (float) RAND_MAX) < crit_chance) ? 1 : 0;
                }
            }
            float crit_mult = 1.0f;
            if (is_crit)
            {
                crit_mult = 1.0f + ((float) player->crit_damage * 0.01f);
                if (crit_mult > 5.0f)
                    crit_mult = 5.0f;
            }

            /* Phase 10: Damage Application and Mitigation */
            int health_before = enemies[i].health;
            int final_dmg = 0;
            int overkill_accum = 0;

            /* Physical damage component with armor penetration */
            if (part_phys > 0.01f)
            {
                int comp_raw = (int) floorf(part_phys + 0.5f);
                if (comp_raw < 1)
                    comp_raw = 1;
                if (is_crit && g_crit_layering_mode == 0)
                {
                    float cval = (float) comp_raw * crit_mult;
                    comp_raw = (int) floorf(cval + 0.5f);
                    if (comp_raw < 1)
                        comp_raw = 1;
                }
                /* Armor penetration calculation */
                int eff_armor = enemies[i].armor;
                int pen_flat = player->pen_flat;
                if (pen_flat > 0)
                {
                    eff_armor -= pen_flat;
                    if (eff_armor < 0)
                        eff_armor = 0;
                }
                int pen_pct = player->pen_percent;
                if (pen_pct > 100)
                    pen_pct = 100;
                if (pen_pct > 0)
                {
                    int reduce = (enemies[i].armor * pen_pct) / 100;
                    eff_armor -= reduce;
                    if (eff_armor < 0)
                        eff_armor = 0;
                }
                int local_overkill = 0;
                int mitig = _rogue_apply_mitig_with_override_armor(
                    &enemies[i], comp_raw, ROGUE_DMG_PHYSICAL, eff_armor, &local_overkill);
                if (is_crit && g_crit_layering_mode == 1)
                {
                    float cval2 = (float) mitig * crit_mult;
                    mitig = (int) floorf(cval2 + 0.5f);
                    if (mitig < 1)
                        mitig = 1;
                }
                enemies[i].health -= mitig;
                final_dmg += mitig;
                overkill_accum += local_overkill;
                rogue_add_damage_number_ex(ex, ey - 0.25f, mitig, 1, is_crit);
                /* Emit component event: physical */
                rogue_damage_event_record(
                    (unsigned short) (def ? def->id : 0), (unsigned char) ROGUE_DMG_PHYSICAL,
                    (unsigned char) is_crit, comp_raw, mitig, local_overkill, 0);
            }

            /* Fire damage component */
            if (part_fire > 0.01f)
            {
                int comp_raw = (int) floorf(part_fire + 0.5f);
                if (comp_raw < 1)
                    comp_raw = 1;
                if (is_crit && g_crit_layering_mode == 0)
                {
                    float cval = (float) comp_raw * crit_mult;
                    comp_raw = (int) floorf(cval + 0.5f);
                    if (comp_raw < 1)
                        comp_raw = 1;
                }
                int local_overkill = 0;
                int mitig = rogue_apply_mitigation_enemy(&enemies[i], comp_raw, ROGUE_DMG_FIRE,
                                                         &local_overkill);
                if (is_crit && g_crit_layering_mode == 1)
                {
                    float cval2 = (float) mitig * crit_mult;
                    mitig = (int) floorf(cval2 + 0.5f);
                    if (mitig < 1)
                        mitig = 1;
                }
                enemies[i].health -= mitig;
                final_dmg += mitig;
                overkill_accum += local_overkill;
                rogue_add_damage_number_ex(ex, ey - 0.25f, mitig, 1, is_crit);
                /* Emit component event: fire */
                rogue_damage_event_record((unsigned short) (def ? def->id : 0),
                                          (unsigned char) ROGUE_DMG_FIRE, (unsigned char) is_crit,
                                          comp_raw, mitig, local_overkill, 0);
            }

            /* Frost damage component */
            if (part_frost > 0.01f)
            {
                int comp_raw = (int) floorf(part_frost + 0.5f);
                if (comp_raw < 1)
                    comp_raw = 1;
                if (is_crit && g_crit_layering_mode == 0)
                {
                    float cval = (float) comp_raw * crit_mult;
                    comp_raw = (int) floorf(cval + 0.5f);
                    if (comp_raw < 1)
                        comp_raw = 1;
                }
                int local_overkill = 0;
                int mitig = rogue_apply_mitigation_enemy(&enemies[i], comp_raw, ROGUE_DMG_FROST,
                                                         &local_overkill);
                if (is_crit && g_crit_layering_mode == 1)
                {
                    float cval2 = (float) mitig * crit_mult;
                    mitig = (int) floorf(cval2 + 0.5f);
                    if (mitig < 1)
                        mitig = 1;
                }
                enemies[i].health -= mitig;
                final_dmg += mitig;
                overkill_accum += local_overkill;
                rogue_add_damage_number_ex(ex, ey - 0.25f, mitig, 1, is_crit);
                /* Emit component event: frost */
                rogue_damage_event_record((unsigned short) (def ? def->id : 0),
                                          (unsigned char) ROGUE_DMG_FROST, (unsigned char) is_crit,
                                          comp_raw, mitig, local_overkill, 0);
            }

            /* Arcane damage component */
            if (part_arc > 0.01f)
            {
                int comp_raw = (int) floorf(part_arc + 0.5f);
                if (comp_raw < 1)
                    comp_raw = 1;
                if (is_crit && g_crit_layering_mode == 0)
                {
                    float cval = (float) comp_raw * crit_mult;
                    comp_raw = (int) floorf(cval + 0.5f);
                    if (comp_raw < 1)
                        comp_raw = 1;
                }
                int local_overkill = 0;
                int mitig = rogue_apply_mitigation_enemy(&enemies[i], comp_raw, ROGUE_DMG_ARCANE,
                                                         &local_overkill);
                if (is_crit && g_crit_layering_mode == 1)
                {
                    float cval2 = (float) mitig * crit_mult;
                    mitig = (int) floorf(cval2 + 0.5f);
                    if (mitig < 1)
                        mitig = 1;
                }
                enemies[i].health -= mitig;
                final_dmg += mitig;
                overkill_accum += local_overkill;
                rogue_add_damage_number_ex(ex, ey - 0.25f, mitig, 1, is_crit);
                /* Emit component event: arcane */
                rogue_damage_event_record((unsigned short) (def ? def->id : 0),
                                          (unsigned char) ROGUE_DMG_ARCANE, (unsigned char) is_crit,
                                          comp_raw, mitig, local_overkill, 0);
            }

            /* Phase 11: Execution Detection */
            int overkill = overkill_accum;
            int execution = 0;
            if (health_before > 0)
            {
                int health_after = enemies[i].health;
                if (health_after <= 0)
                {
                    float health_pct_before =
                        (float) health_before /
                        (float) (enemies[i].max_health > 0 ? enemies[i].max_health : 1);
                    float overkill_pct =
                        (float) overkill /
                        (float) (enemies[i].max_health > 0 ? enemies[i].max_health : 1);
                    if (health_pct_before <= ROGUE_EXEC_HEALTH_PCT ||
                        overkill_pct >= ROGUE_EXEC_OVERKILL_PCT)
                    {
                        execution = 1;
                    }
                }
            }

            /* Phase 12: Event Emission and Hit Feedback */
            /* Emit a single composite damage event per hit (sum of all components). Use the
             * attack's declared damage type for now. */
            rogue_damage_event_record((unsigned short) (def ? def->id : 0),
                                      (unsigned char) (def ? def->damage_type : ROGUE_DMG_PHYSICAL),
                                      (unsigned char) is_crit, (int) floorf(raw_total + 0.5f),
                                      final_dmg, overkill, (unsigned char) execution);

            /* Basic hit feedback */
            enemies[i].hurt_timer = 150.0f;
            enemies[i].flash_timer = 90.0f;
            pc->hit_confirmed = 1;

            /* Phase 13: Advanced Hit Effects */
            /* Phase 4 full feedback: refined knockback, SFX (first hit), particles, overkill
             * explosion flag */
            extern int g_app_frame_audio_guard; /* optional future global */
            int first_hit = (si == 0);
            const RogueHitDebugFrame* dbg = rogue_hit_debug_last();
            int idx_in_dbg = -1;
            if (dbg)
            {
                for (int di = 0; di < dbg->hit_count; ++di)
                {
                    if (dbg->last_hits[di] == i)
                    {
                        idx_in_dbg = di;
                        break;
                    }
                }
            }
            float nx = 0, ny = 1;
            if (idx_in_dbg >= 0)
            {
                nx = dbg->normals[idx_in_dbg][0];
                ny = dbg->normals[idx_in_dbg][1];
            }
            /* Refined magnitude uses level+strength differential (enemy->level may be 0 default) */
            float mag = rogue_hit_calc_knockback_mag(
                player->level, enemies[i].level, player->strength,
                enemies[i].armor /* reuse armor as pseudo strength if enemy stat absent */);
            /* Keep targets stationary during forced-attack test harness runs to allow repeated
             * strikes without drift. */
            if (!rogue_force_attack_active)
            {
                enemies[i].base.pos.x += nx * mag;
                enemies[i].base.pos.y += ny * mag;
            }
            if (!first_strike_enemy_processed)
            {
                rogue_app_add_hitstop(55.0f);
                first_strike_enemy_processed = 1;
            }
            int was_overkill = execution; /* treat execution as overkill for explosion path */
            if (first_hit)
            {
                rogue_hit_play_impact_sfx(player->equipped_weapon_id, is_crit ? 1 : 0);
            }
            rogue_hit_particles_spawn_impact(ex, ey, nx, ny, was_overkill);
            if (was_overkill)
            {
                rogue_hit_mark_explosion();
            }

            /* Phase 14: Status Effect Buildup */
            if (bleed_build > 0)
            {
                enemies[i].bleed_buildup += bleed_build;
            }
            if (frost_build > 0)
            {
                enemies[i].frost_buildup += frost_build;
            }

            /* Phase 15: Poise Damage and Staggering */
            if (def && def->poise_damage > 0.0f && enemies[i].poise_max > 0.0f)
            {
                float poise_dmg = def->poise_damage;
                if (wdef)
                {
                    poise_dmg *= wdef->poise_damage_mult;
                }
                poise_dmg *= sm.poise_damage_mult;
                if (inf)
                {
                    poise_dmg *= inf->phys_scalar;
                }
                enemies[i].poise -= poise_dmg;
                if (enemies[i].poise < 0.0f)
                {
                    enemies[i].poise = 0.0f;
                }
                if (enemies[i].poise <= 0.0f && !enemies[i].staggered)
                {
                    enemies[i].staggered = 1;
                    enemies[i].stagger_timer_ms = 600.0f;
                    if (pc->event_count < (int) (sizeof(pc->events) / sizeof(pc->events[0])))
                    {
                        pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_STAGGER_ENEMY;
                        pc->events[pc->event_count].data = (unsigned short) i;
                        pc->events[pc->event_count].t_ms = pc->strike_time_ms;
                        pc->event_count++;
                    }
                }
            }

            /* Phase 16: Death Processing and Weapon Updates */
            if (enemies[i].health <= 0)
            {
                enemies[i].alive = 0;
                kills++;
            }
            if (wdef)
            {
                rogue_weapon_register_hit(wdef->id, (float) final_dmg);
                rogue_weapon_tick_durability(wdef->id, 1.0f);
            }
        }
    }

    /* Phase 17: Window Cleanup and Finalization */
    pc->processed_window_mask |= process_mask;
    if (def)
    {
        for (int wi = 0; wi < def->num_windows && wi < 32; ++wi)
        {
            unsigned int bit = (1u << wi);
            if (process_mask & bit)
            {
                if (pc->event_count < (int) (sizeof(pc->events) / sizeof(pc->events[0])))
                {
                    pc->events[pc->event_count].type = ROGUE_COMBAT_EVENT_END_WINDOW;
                    pc->events[pc->event_count].data = (unsigned short) wi;
                    pc->events[pc->event_count].t_ms = pc->strike_time_ms;
                    pc->event_count++;
                }
            }
        }
    }
    rogue_player_set_hyper_armor_active(0);
    if (pc->pending_charge_damage_mult > 1.0f)
    {
        pc->pending_charge_damage_mult = 1.0f;
    }
    return kills;
}
