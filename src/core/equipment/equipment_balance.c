/**
 * @file equipment_balance.c
 * @brief Equipment balance analysis and variant management system.
 *
 * This file implements the equipment balance system for Phases 11.5-11.6,
 * providing analytics for proc triggers and damage reduction chains, along
 * with a variant system for different balance configurations.
 *
 * Key functionality includes:
 * - Proc trigger analytics and oversaturation detection
 * - Damage reduction chain analysis
 * - Balance variant management and selection
 * - JSON export for analytics data
 * - Deterministic variant selection for testing
 */

#include "equipment_balance.h"
#include <stdio.h>
#include <string.h>

/* ---- Analytics State ---- */

/**
 * @brief Global analytics state for proc and damage reduction tracking.
 *
 * Tracks proc triggers within a rolling window and damage reduction sources
 * for balance analysis.
 */
static int g_proc_triggers_window = 0; /**< Triggers counted since last analyze */
static int g_proc_oversat_flag = 0;    /**< Flag indicating proc oversaturation */
static float g_dr_sources[16];         /**< Array of damage reduction percentages */
static int g_dr_source_count = 0;      /**< Number of damage reduction sources recorded */
static int g_dr_chain_flag = 0;        /**< Flag indicating damage reduction chain issues */

/* ---- Balance Variants ---- */

/**
 * @brief Global balance variant storage and current selection.
 *
 * Manages multiple balance parameter sets and tracks the currently active variant.
 */
static RogueBalanceParams g_variants[ROGUE_BALANCE_VARIANT_CAP]; /**< Array of balance variants */
static int g_variant_count = 0;                                  /**< Number of registered variants */
static const RogueBalanceParams* g_current = NULL;               /**< Currently selected variant */

/**
 * @brief Simple string hashing function using FNV-1a algorithm.
 *
 * Generates a hash value for string-based identifiers used in balance variants.
 *
 * @param s Input string to hash.
 * @return int Hash value of the input string.
 */
static int hash_str(const char* s)
{
    unsigned int h = 2166136261u;
    while (*s)
    {
        h ^= (unsigned char) *s++;
        h *= 16777619u;
    }
    return (int) h;
}

/**
 * @brief Reset all analytics state to initial values.
 *
 * Clears the proc trigger window, damage reduction sources, and all
 * analysis flags. Should be called at the start of each analysis period.
 */
void rogue_equipment_analytics_reset(void)
{
    g_proc_triggers_window = 0;
    g_proc_oversat_flag = 0;
    g_dr_source_count = 0;
    g_dr_chain_flag = 0;
}
/**
 * @brief Record a proc trigger event for analytics.
 *
 * Increments the proc trigger counter for the current analysis window.
 * The magnitude parameter is currently unused but reserved for future
 * enhancements.
 *
 * @param magnitude Magnitude of the proc trigger (currently unused).
 */
void rogue_equipment_analytics_record_proc_trigger(int magnitude)
{
    (void) magnitude;
    g_proc_triggers_window++;
}
/**
 * @brief Record a damage reduction source for analytics.
 *
 * Adds a damage reduction percentage to the analysis sources array.
 * Sources are capped at 95% to prevent unrealistic values, and the
 * array has a fixed capacity of 16 sources.
 *
 * @param reduction_pct Damage reduction percentage (0-95).
 */
void rogue_equipment_analytics_record_dr_source(float reduction_pct)
{
    if (g_dr_source_count < (int) (sizeof g_dr_sources / sizeof g_dr_sources[0]))
        g_dr_sources[g_dr_source_count++] = reduction_pct;
}

/**
 * @brief Ensure a default balance variant exists.
 *
 * Creates a default balance variant if no variants are registered.
 * The default variant uses conservative thresholds for outlier detection
 * and balance analysis.
 */
void rogue_balance_ensure_default(void)
{
    if (g_variant_count == 0)
    {
        RogueBalanceParams def = {0};
        snprintf(def.id, sizeof def.id, "default");
        def.id_hash = hash_str(def.id);
        def.outlier_mad_mult = 5;
        def.proc_oversat_threshold = 20;
        def.dr_chain_floor = 0.2f;
        g_variants[g_variant_count++] = def;
        g_current = &g_variants[0];
    }
    if (!g_current && g_variant_count > 0)
        g_current = &g_variants[0];
}

/**
 * @brief Register a new balance variant.
 *
 * Adds a new balance parameter set to the variant registry. The variant
 * must have a valid ID and there must be space in the registry. Duplicate
 * IDs are not allowed.
 *
 * @param p Pointer to balance parameters to register.
 * @return int Index of registered variant on success, negative error code on failure:
 *         -1: Invalid parameters (NULL or empty ID)
 *         -2: Registry full
 *         -3: Duplicate ID
 */
int rogue_balance_register(const RogueBalanceParams* p)
{
    if (!p || !p->id[0])
        return -1;
    if (g_variant_count >= ROGUE_BALANCE_VARIANT_CAP)
        return -2; /* prevent duplicate id */
    for (int i = 0; i < g_variant_count; i++)
    {
        if (strcmp(g_variants[i].id, p->id) == 0)
            return -3;
    }
    g_variants[g_variant_count] = *p;
    g_variants[g_variant_count].id_hash = hash_str(p->id);
    if (!g_current)
        g_current = &g_variants[g_variant_count];
    return g_variant_count++;
}
/**
 * @brief Get the number of registered balance variants.
 *
 * @return int Number of balance variants currently registered.
 */
int rogue_balance_variant_count(void) { return g_variant_count; }

/**
 * @brief Deterministically select a balance variant using a seed.
 *
 * Uses a hash-based approach to select a balance variant based on the
 * provided seed. This ensures reproducible variant selection for testing
 * and balance validation.
 *
 * @param seed Seed value for deterministic selection.
 * @return int Index of selected variant, or -1 if no variants available.
 */
int rogue_balance_select_deterministic(unsigned int seed)
{
    if (g_variant_count == 0)
    {
        rogue_balance_ensure_default();
    }
    if (g_variant_count == 0)
        return -1;
    unsigned int h = seed;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    int idx = (int) (h % (unsigned int) g_variant_count);
    g_current = &g_variants[idx];
    return idx;
}
/**
 * @brief Get the currently selected balance variant.
 *
 * Returns a pointer to the currently active balance parameters.
 * Ensures a default variant exists if none is selected.
 *
 * @return const RogueBalanceParams* Pointer to current balance parameters.
 */
const RogueBalanceParams* rogue_balance_current(void)
{
    rogue_balance_ensure_default();
    return g_current;
}

/**
 * @brief Perform balance analysis on collected analytics data.
 *
 * Analyzes the collected proc triggers and damage reduction sources
 * against the current balance parameters to detect:
 * - Proc oversaturation (too many triggers)
 * - Damage reduction chain issues (excessive total reduction)
 *
 * Resets the rolling counters after analysis.
 */
void rogue_equipment_analytics_analyze(void)
{
    rogue_balance_ensure_default();
    const RogueBalanceParams* cfg = g_current;
    if (!cfg)
        return; /* Proc oversaturation: simple threshold */
    g_proc_oversat_flag =
        (g_proc_triggers_window > cfg->proc_oversat_threshold)
            ? 1
            : 0; /* DR chain: compute cumulative remaining damage fraction after applying sources
                    sequentially as (1 - r/100). If result < floor => flag */
    float remain = 1.0f;
    for (int i = 0; i < g_dr_source_count; i++)
    {
        float r = g_dr_sources[i];
        if (r < 0)
            r = 0;
        if (r > 95.f)
            r = 95.f;
        remain *= (1.f - r / 100.f);
    }
    g_dr_chain_flag = (remain < cfg->dr_chain_floor) ? 1 : 0; /* reset rolling counters (window) */
    g_proc_triggers_window = 0;
    g_dr_source_count = 0;
}

/**
 * @brief Check if proc oversaturation was detected.
 *
 * @return int 1 if proc oversaturation detected, 0 otherwise.
 */
int rogue_equipment_analytics_flag_proc_oversat(void) { return g_proc_oversat_flag; }

/**
 * @brief Check if damage reduction chain issues were detected.
 *
 * @return int 1 if damage reduction chain issues detected, 0 otherwise.
 */
int rogue_equipment_analytics_flag_dr_chain(void) { return g_dr_chain_flag; }

/**
 * @brief Export analytics results as JSON.
 *
 * Formats the current analytics flags into a JSON object string.
 *
 * @param buf Output buffer for JSON string.
 * @param cap Capacity of output buffer.
 * @return int Number of characters written on success, -1 on failure.
 */
int rogue_equipment_analytics_export_json(char* buf, int cap)
{
    if (!buf || cap < 8)
        return -1;
    int n = snprintf(buf, (size_t) cap, "{\"proc_oversaturation\":%d,\"dr_chain\":%d}",
                     g_proc_oversat_flag, g_dr_chain_flag);
    if (n <= 0 || n >= cap)
        return -1;
    return n;
}
