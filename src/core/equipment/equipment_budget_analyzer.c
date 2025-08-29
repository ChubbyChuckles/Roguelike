/**
 * @file equipment_budget_analyzer.c
 * @brief Equipment budget utilization analysis system (Phase 16.5).
 *
 * This file implements the budget analyzer for equipment items, providing
 * comprehensive analysis of how effectively items utilize their allocated
 * budget across different utilization ranges. The system tracks item
 * budget utilization, identifies over-budget items, and provides statistical
 * analysis for game balance tuning.
 *
 * Key functionality includes:
 * - Budget utilization analysis across all active items
 * - Statistical reporting (average, maximum utilization)
 * - Bucket-based distribution analysis
 * - JSON export for external analysis tools
 * - Over-budget item detection
 */

#include "equipment_budget_analyzer.h"
#include "../loot/loot_instances.h" /* for item instances & budget helpers */
#include <stdio.h>
#include <string.h>

/**
 * @brief Global state for budget analysis reports.
 *
 * Stores the most recent budget analysis report and tracks whether
 * a valid report has been generated.
 */
static RogueBudgetReport g_last_report; /**< Most recent budget analysis report */
static int g_has_report = 0;            /**< Flag indicating if a report is available */

/**
 * @brief Reset the budget analyzer state.
 *
 * Clears the last report and resets the analyzer to its initial state.
 * Should be called before running a new analysis or when clearing cached data.
 */
void rogue_budget_analyzer_reset(void)
{
    memset(&g_last_report, 0, sizeof(g_last_report));
    g_has_report = 0;
}

/**
 * @brief Determine the utilization bucket index for a given ratio.
 *
 * Categorizes budget utilization ratios into predefined buckets for
 * statistical analysis. The buckets represent different utilization ranges:
 * - 0: < 25% utilization
 * - 1: 25-49% utilization
 * - 2: 50-74% utilization
 * - 3: 75-89% utilization
 * - 4: 90-100% utilization
 * - 5: > 100% utilization (over budget)
 *
 * @param ratio Budget utilization ratio (weight / max_budget).
 * @return int Bucket index (0-5), or 5 for ratios > 1.0.
 */
static int bucket_index(float ratio)
{
    if (ratio < 0.25f)
        return 0;
    if (ratio < 0.5f)
        return 1;
    if (ratio < 0.75f)
        return 2;
    if (ratio < 0.90f)
        return 3;
    if (ratio <= 1.0f)
        return 4;
    return 5;
}

/**
 * @brief Run budget utilization analysis on all active items.
 *
 * Performs a comprehensive analysis of all active item instances, calculating
 * budget utilization ratios, identifying over-budget items, and generating
 * statistical summaries. The analysis includes:
 * - Average and maximum utilization ratios
 * - Count of items exceeding budget
 * - Distribution across utilization buckets
 * - Identification of the most over-utilized item
 *
 * Results are stored globally and optionally copied to the output parameter.
 *
 * @param out Optional output parameter to receive the analysis report.
 */
void rogue_budget_analyzer_run(RogueBudgetReport* out)
{
    RogueBudgetReport r;
    memset(&r, 0, sizeof(r));
    int count_cap = ROGUE_ITEM_INSTANCE_CAP; /* iterate full pool; use active instances */
    for (int i = 0; i < count_cap; i++)
    {
        const RogueItemInstance* inst = rogue_item_instance_at(i);
        if (!inst)
            continue; /* skip inactive */
        float max_budget = (float) rogue_budget_max(inst->item_level, inst->rarity);
        if (max_budget <= 0.0f)
            continue; /* skip invalid config */
        int weight = rogue_item_instance_total_affix_weight(i);
        float ratio = max_budget > 0 ? (float) weight / max_budget : 0.f;
        if (ratio > r.max_utilization)
        {
            r.max_utilization = ratio;
            r.max_item_index = i;
        }
        r.avg_utilization += ratio;
        if (weight > (int) max_budget)
            r.over_budget_count++;
        int bi = bucket_index(ratio);
        if (bi >= 0 && bi < 6)
            r.bucket_counts[bi]++;
        r.item_count++;
    }
    if (r.item_count > 0)
        r.avg_utilization /= (float) r.item_count;
    else
        r.avg_utilization = 0.f;
    g_last_report = r;
    g_has_report = 1;
    if (out)
        *out = r;
}

/**
 * @brief Get the most recent budget analysis report.
 *
 * Returns a pointer to the last generated budget report, or NULL if
 * no analysis has been run yet.
 *
 * @return const RogueBudgetReport* Pointer to last report, or NULL if none available.
 */
const RogueBudgetReport* rogue_budget_analyzer_last(void)
{
    return g_has_report ? &g_last_report : 0;
}

/**
 * @brief Export the last budget analysis report as JSON.
 *
 * Formats the most recent budget analysis report into a JSON object
 * with the following structure:
 * ```json
 * {
 *   "item_count": <number>,
 *   "over_budget_count": <number>,
 *   "avg_utilization": <float>,
 *   "max_utilization": <float>,
 *   "max_item_index": <number>,
 *   "buckets": {
 *     "lt25": <number>,
 *     "lt50": <number>,
 *     "lt75": <number>,
 *     "lt90": <number>,
 *     "le100": <number>,
 *     "gt100": <number>
 *   }
 * }
 * ```
 *
 * @param buf Output buffer for JSON string.
 * @param cap Capacity of output buffer in bytes.
 * @return int Number of characters written (excluding null terminator) on success,
 *         -1 on buffer error, 0 if no report available.
 */
int rogue_budget_analyzer_export_json(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    if (!g_has_report)
    {
        if (cap > 0)
            buf[0] = '\0';
        return 0;
    }
    const RogueBudgetReport* r = &g_last_report;
    /* deterministic key order */
    int n =
        snprintf(buf, (size_t) cap,
                 "{\n"
                 "  \"item_count\":%d,\n"
                 "  \"over_budget_count\":%d,\n"
                 "  \"avg_utilization\":%.4f,\n"
                 "  \"max_utilization\":%.4f,\n"
                 "  \"max_item_index\":%d,\n"
                 "  \"buckets\":{\n"
                 "    \"lt25\":%d,\n"
                 "    \"lt50\":%d,\n"
                 "    \"lt75\":%d,\n"
                 "    \"lt90\":%d,\n"
                 "    \"le100\":%d,\n"
                 "    \"gt100\":%d\n"
                 "  }\n"
                 "}\n",
                 r->item_count, r->over_budget_count, r->avg_utilization, r->max_utilization,
                 r->max_item_index, r->bucket_counts[0], r->bucket_counts[1], r->bucket_counts[2],
                 r->bucket_counts[3], r->bucket_counts[4], r->bucket_counts[5]);
    if (n >= cap)
    { /* truncated */
        if (cap > 0)
            buf[cap - 1] = '\0';
    }
    return n;
}
