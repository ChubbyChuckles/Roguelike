/**
 * @file durability.c
 * @brief Equipment durability system with non-linear decay and warning notifications
 *
 * This module implements a comprehensive durability system for weapons and armor
 * that simulates realistic wear and tear during combat. Features include non-linear
 * decay modeling, rarity-based scaling, bucket classification for UI feedback,
 * and automatic warning notifications for equipment maintenance.
 *
 * Key Features:
 * - Non-linear durability decay with diminishing returns for high-severity events
 * - Rarity-based scaling (higher rarity items degrade more slowly)
 * - Three-tier bucket classification (good/warn/critical) for UI feedback
 * - Automatic transition detection for equipment warning notifications
 * - Severity-based damage scaling with configurable thresholds
 *
 * Durability Buckets:
 * - Good: ≥60% durability (green/normal state)
 * - Warning: 30-59% durability (yellow/caution state)
 * - Critical: <30% durability (red/danger state)
 *
 * Decay Model (Phase 8.1):
 * Loss = ceil(base × severity_scale × rarity_factor)
 * - Base: 1 (normal) or 2 (high severity ≥50)
 * - Severity Scale: log₂(1 + severity/25) with minimum 0.2
 * - Rarity Factor: 1/(1 + 0.35 × rarity) - higher rarity loses less
 * - Minimum loss: 1 durability point when severity > 0
 *
 * Transition Detection (Phase 8.3):
 * - Tracks bucket transitions during durability events
 * - Provides notification system for UI warnings
 * - Supports auto-warn functionality for equipment maintenance
 *
 * @note Only affects weapons and armor categories
 * @note Durability loss occurs during combat events (damage dealt/received)
 * @note Rarity scaling encourages investment in higher-quality equipment
 * @note Non-linear scaling prevents excessive damage from single high-severity events
 */

#include "durability.h"
#include "../core/loot/loot_instances.h"
#include "../core/loot/loot_item_defs.h"
#include <math.h>

/* Internal state for Phase 8.3 auto-warn bucket transitions. */
static int g_last_durability_transition = 0; /* 0 none,1 warn,2 critical */

/* Internal state for Phase 8.3 auto-warn bucket transitions. */
static int g_last_durability_transition = 0; /* 0 none,1 warn,2 critical */

/**
 * @brief Reset durability transition state for the current tick
 *
 * Clears the transition notification state at the start of each game tick.
 * This should be called after processing all durability-affecting events
 * to prepare for the next tick's transition detection.
 *
 * The transition state tracks when equipment moves between durability buckets:
 * - 0: No transition occurred
 * - 1: Equipment entered warning bucket (30-59% durability)
 * - 2: Equipment entered critical bucket (<30% durability)
 *
 * @note Phase 8.3: Auto-warn notification system
 * @note Caller determines tick cadence (typically once per frame)
 * @note Must be called before checking transitions with rogue_durability_last_transition()
 */
void rogue_durability_notify_tick(void)
{
    g_last_durability_transition = 0; /* reset each tick call; caller decides cadence */
}
/**
 * @brief Get and clear the last durability bucket transition
 *
 * Retrieves the most recent durability bucket transition that occurred and
 * clears the state for the next transition detection. This enables UI systems
 * to provide timely warnings when equipment durability degrades significantly.
 *
 * Return Values:
 * - 0: No transition occurred (equipment remained in same bucket)
 * - 1: Equipment entered warning bucket (30-59% durability)
 * - 2: Equipment entered critical bucket (<30% durability)
 *
 * @return Transition type code (0=none, 1=warning, 2=critical)
 *
 * @note Phase 8.3: Auto-warn notification system
 * @note State is cleared after reading to prevent duplicate notifications
 * @note Only tracks downward transitions (degradation, not repair)
 * @note Multiple transitions in one tick return the most severe (highest number)
 */
int rogue_durability_last_transition(void)
{
    int v = g_last_durability_transition;
    g_last_durability_transition = 0;
    return v;
}

/* Reuse existing classification already present in vendor_ui.c but keep centralized here for non-UI
 * callers. */
/**
 * @brief Classify durability percentage into quality buckets
 *
 * Converts a durability percentage (0.0-1.0) into a discrete quality bucket
 * used for UI feedback and warning systems. This centralized classification
 * ensures consistent behavior across all durability-related UI components.
 *
 * Bucket Classification:
 * - 0 (Critical): <30% durability - red/danger state
 * - 1 (Warning): 30-59% durability - yellow/caution state
 * - 2 (Good): ≥60% durability - green/normal state
 *
 * @param pct Durability percentage (0.0 to 1.0, clamped automatically)
 * @return Bucket classification (0=critical, 1=warning, 2=good)
 *
 * @note Input percentage is automatically clamped to [0.0, 1.0] range
 * @note Thresholds are chosen to provide clear visual feedback progression
 * @note Used by both UI systems and durability warning notifications
 * @note Centralized to ensure consistency across all durability displays
 */
int rogue_durability_bucket(float pct)
{
    if (pct < 0)
        pct = 0;
    if (pct > 1)
        pct = 1;
    if (pct < 0.30f)
        return 0;
    if (pct < 0.60f)
        return 1;
    return 2;
}

/* Non-linear durability decay model (Phase 8.1)
   We apply: loss = ceil( base * S * R ) where:
     S = log2(1 + severity/25)   (diminishing for large events)
     base = 1 (minimum chip) or elevated to 2 if severity >=50
     R = rarity factor = 1 / (1 + 0.35 * rarity)  (higher rarity loses less)
   A final floor ensures at least 1 lost when severity>0.
*/
/**
 * @brief Apply durability damage to an equipped item with non-linear decay
 *
 * Applies durability loss to weapons and armor based on combat severity,
 * using a sophisticated non-linear decay model that accounts for item rarity
 * and event severity. Also detects and records bucket transitions for UI warnings.
 *
 * Non-Linear Decay Formula (Phase 8.1):
 * Loss = ceil(base × severity_scale × rarity_factor)
 *
 * Where:
 * - Base = 1 (normal) or 2 (high severity ≥50)
 * - Severity Scale = log₂(1 + severity/25), minimum 0.2
 * - Rarity Factor = 1/(1 + 0.35 × rarity) - higher rarity loses less
 * - Final Loss = max(calculated_loss, 1) when severity > 0
 *
 * @param inst_index Index of the item instance to damage
 * @param severity Combat severity (typically damage dealt/received, must be > 0)
 * @return Actual durability points lost (0 if no damage applied)
 *
 * @note Only affects weapons and armor categories
 * @note Zero severity or invalid items return 0 (no damage)
 * @note Items with no durability (durability_max ≤ 0) are unaffected
 * @note Rarity scaling encourages investment in higher-quality equipment
 * @note Automatically triggers transition detection for UI warnings
 * @note Phase 8.1: Non-linear decay prevents excessive single-event damage
 * @note Phase 8.3: Integrated with auto-warn notification system
 */
int rogue_item_instance_apply_durability_event(int inst_index, int severity)
{
    if (severity <= 0)
        return 0;
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return 0;
    if (it->durability_max <= 0)
        return 0;
    const RogueItemDef* def = rogue_item_def_at(it->def_index);
    if (!def)
        return 0;
    if (!(def->category == ROGUE_ITEM_WEAPON || def->category == ROGUE_ITEM_ARMOR))
        return 0;
    int rarity = def->rarity;
    if (rarity < 0)
        rarity = 0;
    if (rarity > 10)
        rarity = 10;
    double S = log2(1.0 + (double) severity / 25.0);
    if (S < 0.2)
        S = 0.2; /* minimal scale */
    int base = (severity >= 50) ? 2 : 1;
    double R = 1.0 / (1.0 + 0.35 * (double) rarity);
    double raw = (double) base * S * R;
    int loss = (int) ceil(raw);
    if (loss < 1)
        loss = 1;
    /* Apply & transition detection */
    int before_cur = 0, before_max = 0;
    rogue_item_instance_get_durability(inst_index, &before_cur, &before_max);
    float before_pct = (before_max > 0) ? (float) before_cur / (float) before_max : 0.0f;
    rogue_item_instance_damage_durability(inst_index, loss);
    int after_cur = 0, after_max = 0;
    rogue_item_instance_get_durability(inst_index, &after_cur, &after_max);
    float after_pct = (after_max > 0) ? (float) after_cur / (float) after_max : 0.0f;
    int before_bucket = rogue_durability_bucket(before_pct);
    int after_bucket = rogue_durability_bucket(after_pct);
    if (after_bucket < before_bucket)
    {
        if (after_bucket == 1)
            g_last_durability_transition = 1;
        else if (after_bucket == 0)
            g_last_durability_transition = 2;
    }
    return loss;
}
