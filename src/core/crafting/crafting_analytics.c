/**
 * @file crafting_analytics.c
 * @brief Lightweight in-memory analytics for crafting & gathering.
 *
 * Collects session-scoped counters and histograms for craft quality,
 * gather node activity, enhancement attempts and material flow.
 * Designed for low-overhead runtime telemetry and JSON export.
 */
#include "crafting_analytics.h"
#include <stdio.h>
#include <string.h>

#ifndef ROGUE_MATERIAL_DEF_CAP
#define ROGUE_MATERIAL_DEF_CAP 512
#endif

/* Session timing and node harvest counters */
static unsigned int s_session_start_ms = 0;
static unsigned int s_last_event_ms = 0;
static unsigned int s_total_nodes = 0;
static unsigned int s_rare_nodes = 0;

/* Crafting event counters and quality histogram (0..100 inclusive) */
static unsigned int s_craft_events = 0;
static unsigned int s_craft_success = 0;
static unsigned int s_quality_hist[101]; /* 0..100 inclusive */

/* Enhancement attempt tracking */
static unsigned int s_enh_attempts = 0;
static double s_enh_expected_accum = 0.0; /* sum of expected risks */
static unsigned int s_enh_failures = 0;   /* treat failure if success_flag==0 */

/* Material acquire/consume counters per material_def */
static unsigned long long s_material_acquire[ROGUE_MATERIAL_DEF_CAP];
static unsigned long long s_material_consume[ROGUE_MATERIAL_DEF_CAP];

/* Drift alert latch to prevent repeated alerts */
static int s_drift_alert_latched = 0;

/**
 * @brief Reset all in-memory analytics counters to initial state.
 *
 * Clears histograms and counters. Typically called at session start
 * or when telemetry should be restarted.
 */
void rogue_craft_analytics_reset(void)
{
    s_session_start_ms = 0;
    s_last_event_ms = 0;
    s_total_nodes = 0;
    s_rare_nodes = 0;
    s_craft_events = 0;
    s_craft_success = 0;
    memset(s_quality_hist, 0, sizeof s_quality_hist);
    s_enh_attempts = 0;
    s_enh_expected_accum = 0.0;
    s_enh_failures = 0;
    memset(s_material_acquire, 0, sizeof s_material_acquire);
    memset(s_material_consume, 0, sizeof s_material_consume);
    s_drift_alert_latched = 0;
}

/**
 * @brief Record a gather node harvest event.
 *
 * Increments session counters for nodes visited and rare node
 * occurrences. The node/material/qty parameters are accepted for
 * future extension but are not currently used aside from logging
 * timing information.
 *
 * @param node_def_index Index of the gather node definition
 * @param material_def_index Material definition index harvested
 * @param qty Quantity harvested
 * @param rare_flag Non-zero when this was a rare node proc
 * @param now_ms Current timestamp in milliseconds
 */
void rogue_craft_analytics_on_node_harvest(int node_def_index, int material_def_index, int qty,
                                           int rare_flag, unsigned int now_ms)
{
    (void) node_def_index;
    (void) material_def_index;
    (void) qty;
    if (s_session_start_ms == 0)
        s_session_start_ms = now_ms;
    s_last_event_ms = now_ms;
    s_total_nodes++;
    if (rare_flag)
        s_rare_nodes++;
}

/**
 * @brief Record a crafting event for analytics.
 *
 * Updates event counters and the quality histogram. The recipe
 * index is currently unused but left in the signature for later
 * grouping by recipe.
 *
 * @param recipe_index Index of the recipe used
 * @param quality_out Resulting quality value (0..100)
 * @param success_flag Non-zero if craft succeeded
 */
void rogue_craft_analytics_on_craft(int recipe_index, int quality_out, int success_flag)
{
    (void) recipe_index;
    if (quality_out < 0)
        quality_out = 0;
    if (quality_out > 100)
        quality_out = 100;
    s_craft_events++;
    if (success_flag)
        s_craft_success++;
    s_quality_hist[quality_out]++;
}

/**
 * @brief Record an enhancement attempt and accumulate expected risk.
 *
 * expected_risk should be a probability in [0,1] representing the
 * estimated chance of failure; this is accumulated to later compute
 * observed vs expected failure rates.
 *
 * @param expected_risk Expected failure probability (clamped 0..1)
 * @param success_flag Non-zero if enhancement succeeded
 */
void rogue_craft_analytics_on_enhancement(float expected_risk, int success_flag)
{
    if (expected_risk < 0)
        expected_risk = 0;
    if (expected_risk > 1)
        expected_risk = 1;
    s_enh_attempts++;
    s_enh_expected_accum += expected_risk;
    if (!success_flag)
        s_enh_failures++;
}

/**
 * @brief Increment acquire counter for a material_def.
 *
 * @param material_def_index Material definition index
 * @param qty Quantity acquired (positive)
 */
void rogue_craft_analytics_material_acquire(int material_def_index, int qty)
{
    if (material_def_index >= 0 && material_def_index < ROGUE_MATERIAL_DEF_CAP && qty > 0)
        s_material_acquire[material_def_index] += (unsigned long long) qty;
}
/**
 * @brief Increment consume counter for a material_def.
 *
 * @param material_def_index Material definition index
 * @param qty Quantity consumed (positive)
 */
void rogue_craft_analytics_material_consume(int material_def_index, int qty)
{
    if (material_def_index >= 0 && material_def_index < ROGUE_MATERIAL_DEF_CAP && qty > 0)
        s_material_consume[material_def_index] += (unsigned long long) qty;
}

/**
 * @brief Compute nodes harvested per hour based on session timing.
 *
 * @param now_ms Current timestamp in milliseconds
 * @return Nodes per hour (0.0 if insufficient data)
 */
float rogue_craft_analytics_nodes_per_hour(unsigned int now_ms)
{
    if (s_session_start_ms == 0 || s_total_nodes == 0)
        return 0.0f;
    unsigned int elapsed = (now_ms - s_session_start_ms);
    if (elapsed == 0)
        return 0.0f;
    double hours = (double) elapsed / (1000.0 * 3600.0);
    return (float) (s_total_nodes / hours);
}
/**
 * @brief Fraction of harvested nodes that were rare procs.
 *
 * @return Rate in [0,1] or 0 when no nodes recorded
 */
float rogue_craft_analytics_rare_proc_rate(void)
{
    if (s_total_nodes == 0)
        return 0.0f;
    return (float) s_rare_nodes / (float) s_total_nodes;
}
/**
 * @brief Compare observed enhancement failure rate to expected.
 *
 * Returns observed_fail - expected_mean where expected_mean is
 * accumulated from reported expected_risk values. Positive values
 * indicate more failures than expected.
 *
 * @return Difference (observed - expected) or 0 if no attempts
 */
float rogue_craft_analytics_enhance_risk_variance(void)
{
    if (s_enh_attempts == 0)
        return 0.0f;
    double expected_mean = s_enh_expected_accum / (double) s_enh_attempts;
    double observed_fail = (double) s_enh_failures / (double) s_enh_attempts;
    return (float) (observed_fail - expected_mean);
}

/**
 * @brief Check for quality drift in craft results.
 *
 * Latches an alert when the observed average quality moves outside
 * a broad band ([25,75]) after at least 20 events. Once latched the
 * check remains true until reset.
 *
 * @return 1 if drift detected or already latched, 0 otherwise
 */
int rogue_craft_analytics_check_quality_drift(void)
{
    if (s_drift_alert_latched)
        return 1;
    if (s_craft_events < 20)
        return 0; /* need sample size */
    /* Compute average quality */
    unsigned long long total_q = 0;
    for (int i = 0; i <= 100; i++)
    {
        total_q += (unsigned long long) i * (unsigned long long) s_quality_hist[i];
    }
    double avg = (double) total_q / (double) s_craft_events;
    /* Expected mid distribution ~50 if uniform; we allow wide band ±25 before alert. */
    if (avg < 25.0 || avg > 75.0)
    {
        s_drift_alert_latched = 1;
        return 1;
    }
    return 0;
}

/**
 * @brief Export collected analytics to a JSON string.
 *
 * Writes a compact JSON representation into `buf` (up to `cap`
 * bytes) and returns the number of bytes written on success, or -1
 * on error or insufficient capacity. The JSON includes node
 * counters, craft/enhancement stats, quality histogram and material
 * flows.
 *
 * @param buf Destination buffer to receive JSON
 * @param cap Capacity of buf in bytes
 * @return Bytes written, or -1 on error
 */
int rogue_craft_analytics_export_json(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    int written = 0;
    int n;
    double qual_avg = 0.0;
    if (s_craft_events)
    {
        unsigned long long tq = 0;
        for (int i = 0; i <= 100; i++)
        {
            tq += (unsigned long long) i * (unsigned long long) s_quality_hist[i];
        }
        qual_avg = (double) tq / (double) s_craft_events;
    }
    n = snprintf(buf + written, cap - written,
                 "{\n  \"nodes_total\":%u,\n  \"nodes_rare\":%u,\n  \"rare_rate\":%.4f,\n  "
                 "\"craft_events\":%u,\n  \"craft_success\":%u,\n  \"enh_attempts\":%u,\n  "
                 "\"enh_expected_mean\":%.4f,\n  \"enh_observed_fail\":%.4f,\n  "
                 "\"enh_risk_variance\":%.4f,\n  \"quality_avg\":%.2f,\n  \"quality_hist\":[",
                 s_total_nodes, s_rare_nodes, rogue_craft_analytics_rare_proc_rate(),
                 s_craft_events, s_craft_success, s_enh_attempts,
                 s_enh_attempts ? (float) (s_enh_expected_accum / (double) s_enh_attempts) : 0.0f,
                 s_enh_attempts ? (float) ((double) s_enh_failures / (double) s_enh_attempts)
                                : 0.0f,
                 rogue_craft_analytics_enhance_risk_variance(), (float) qual_avg);
    if (n < 0)
        return -1;
    written += n;
    if (written >= cap)
        return -1;
    for (int i = 0; i <= 100; i++)
    {
        n = snprintf(buf + written, cap - written, "%u%s", s_quality_hist[i], (i < 100) ? "," : "");
        if (n < 0)
            return -1;
        written += n;
        if (written >= cap)
            return -1;
    }
    n = snprintf(buf + written, cap - written, "],\n  \"drift_alert\":%d,\n  \"materials\":[",
                 s_drift_alert_latched);
    if (n < 0)
        return -1;
    written += n;
    if (written >= cap)
        return -1;
    int first = 1;
    for (int i = 0; i < ROGUE_MATERIAL_DEF_CAP; i++)
    {
        unsigned long long acq = s_material_acquire[i];
        unsigned long long con = s_material_consume[i];
        if (acq || con)
        {
            n = snprintf(buf + written, cap - written, "%s{\"id\":%d,\"acq\":%llu,\"con\":%llu}",
                         first ? "" : ",", i, acq, con);
            if (n < 0)
                return -1;
            written += n;
            if (written >= cap)
                return -1;
            first = 0;
        }
    }
    n = snprintf(buf + written, cap - written, "]\n}\n");
    if (n < 0)
        return -1;
    written += n;
    if (written >= cap)
        return -1;
    return written;
}
