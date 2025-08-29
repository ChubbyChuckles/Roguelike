/**
 * @file damage_calc.c
 * @brief Central damage and cooldown calculation utilities for active skills
 *
 * This module provides centralized calculation functions for skill damage and cooldowns,
 * integrating various progression systems including mastery, specialization, synergy,
 * and stat scaling. Designed to be the single source of truth for skill effect calculations.
 *
 * Key Features:
 * - Skill rank-based damage scaling with stat bonuses
 * - Synergy system integration for elemental power bonuses
 * - Mastery bonus multipliers for skill-specific improvements
 * - Specialization path scaling (POWER for damage, CONTROL for cooldown)
 * - Cooldown reduction through haste ratings with soft caps
 * - Intelligence-based scaling for intellect-focused skills
 *
 * Progression Integration:
 * - Phase 3.6.4: Stat scaling with progression stats (INT for fireball)
 * - Phase 3.6.5: Mastery bonuses integrated into skill output
 * - Phase 3.6.6: Specialization path scalars for damage/cooldown
 * - Haste-based cooldown reduction with 90% minimum effectiveness
 *
 * Calculation Pipeline:
 * 1. Base damage/cooldown from skill definition and rank
 * 2. Apply synergy bonuses (elemental power totals)
 * 3. Add stat scaling bonuses (intelligence, etc.)
 * 4. Multiply by mastery scalar (>= 1.0)
 * 5. Apply specialization path scalar
 * 6. For cooldowns: Apply haste-based reduction with caps
 *
 * @note Designed for easy extension to other skills following same pattern
 * @note All calculations use stable rounding for consistent results
 * @note Negative scaling results are clamped to zero
 * @note Cooldowns have minimum 100ms floor for game balance
 */

#include "damage_calc.h"
#include "../core/app/app_state.h"
#include "../core/progression/progression_mastery.h"
#include "../core/progression/progression_specialization.h"
#include "../core/progression/progression_synergy.h"
#include "stat_cache.h"
#include <math.h>

/**
 * @brief Calculate fireball skill damage with full progression scaling
 *
 * Computes the total damage output for a fireball skill, integrating multiple
 * progression systems for comprehensive scaling. This serves as the template
 * for other skill damage calculations.
 *
 * Damage Calculation Formula:
 * Base Damage = 3 + (skill_rank × 2)
 * Total = Base + Fire_Synergy + INT_Bonus
 * Total = Total × Mastery_Scalar
 * Total = Total × Specialization_Damage_Scalar
 *
 * @param fireball_skill_id The skill ID for the fireball ability
 * @return Total damage value (minimum 0), or 0 if skill invalid/unlearned
 *
 * @note Phase 3.6.4: Intelligence scaling at 25% of total INT stat
 * @note Phase 3.6.5: Mastery bonuses applied multiplicatively
 * @note Phase 3.6.6: Specialization POWER path affects damage scaling
 * @note Fire synergy provides flat damage bonus from related skills
 * @note All scaling uses stable rounding to nearest integer
 * @note Negative results from scaling are clamped to zero
 * @note Unlearned skills (rank <= 0) return 0 damage
 */
int rogue_damage_fireball(int fireball_skill_id)
{
    const RogueSkillDef* def = rogue_skill_get_def(fireball_skill_id);
    const RogueSkillState* st = rogue_skill_get_state(fireball_skill_id);
    if (!def || !st || st->rank <= 0)
        return 0;
    int base = 3 + st->rank * 2;
    int fire_synergy = rogue_skill_synergy_total(ROGUE_SYNERGY_FIRE_POWER);
    /* Phase 3.6.4: effect scaling with progression stats. For Fireball, scale with INT. */
    int int_bonus = 0;
    /* Ensure stat cache reflects current player totals; caller typically maintains it. */
    int_bonus = (int) (g_player_stat_cache.total_intelligence * 0.25f);
    if (int_bonus < 0)
        int_bonus = 0;
    int total = base + fire_synergy + int_bonus;
    /* Phase 3.6.5: integrate mastery bonuses into active skill output. */
    {
        float ms = 1.0f;
        /* mastery scalar is >= 1.0f; multiply and round to nearest int for stability */
        ms = rogue_mastery_bonus_scalar(fireball_skill_id);
        if (ms > 1.0f)
        {
            float scaled = (float) total * ms;
            if (scaled < 0.0f)
                scaled = 0.0f;
            total = (int) (scaled + 0.5f);
        }
    }
    /* Phase 3.6.6: apply specialization path (POWER) scalar. */
    {
        float ds = rogue_specialization_damage_scalar(fireball_skill_id);
        if (ds != 1.0f)
        {
            float scaled = (float) total * ds;
            if (scaled < 0.0f)
                scaled = 0.0f;
            total = (int) (scaled + 0.5f);
        }
    }
    return total;
}

/**
 * @brief Calculate fireball skill cooldown with haste and specialization scaling
 *
 * Computes the effective cooldown for a fireball skill, applying haste-based
 * cooldown reduction and specialization modifiers. Includes soft caps to
 * prevent excessive cooldown reduction.
 *
 * Cooldown Calculation Formula:
 * Base_CD = definition.base_cooldown - ((rank-1) × reduction_per_rank)
 * Effective_CD = Base_CD × (1 - haste_reduction_percent/100)
 * Effective_CD = Effective_CD × specialization_cooldown_scalar
 * Final_CD = max(Effective_CD, 100ms)
 *
 * @param fireball_skill_id The skill ID for the fireball ability
 * @return Cooldown in milliseconds (minimum 100ms), or 0.0 if skill invalid/unlearned
 *
 * @note Haste reduction capped at 90% (never below 10% of original cooldown)
 * @note Phase 3.6.6: Specialization CONTROL path affects cooldown scaling
 * @note Cooldown reduction per rank comes from skill definition
 * @note Uses haste-effective percentage from stat cache
 * @note 100ms minimum prevents instant recasting
 * @note Unlearned skills (rank <= 0) return 0.0 cooldown
 */
float rogue_cooldown_fireball_ms(int fireball_skill_id)
{
    const RogueSkillDef* def = rogue_skill_get_def(fireball_skill_id);
    const RogueSkillState* st = rogue_skill_get_state(fireball_skill_id);
    if (!def || !st || st->rank <= 0)
        return 0.0f;
    float cd = def->base_cooldown_ms - (st->rank - 1) * def->cooldown_reduction_ms_per_rank;
    /* Apply progression-based cooldown reduction soft/hard caps using haste-effective percent. */
    float cdr_eff = rogue_progression_final_cdr((float) g_player_stat_cache.rating_haste_eff_pct);
    if (cdr_eff > 0.0f && cdr_eff < 100.0f)
    {
        float mult = 1.0f - (cdr_eff / 100.0f);
        if (mult < 0.10f)
            mult = 0.10f; /* never below 90% reduction via this path */
        cd *= mult;
    }
    /* Phase 3.6.6: specialization CONTROL reduces cooldown multiplicatively. */
    {
        float cms = rogue_specialization_cooldown_scalar(fireball_skill_id);
        if (cms > 0.0f && cms != 1.0f)
            cd *= cms;
    }
    if (cd < 100.0f)
        cd = 100.0f;
    return cd;
}
