/**
 * @file loot_analytics.c
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 *
 * This file provides loot drop analytics for monitoring and balancing in the rogue-like game.
 * Maintains a ring buffer of drop events (up to ROGUE_LOOT_ANALYTICS_RING_CAP), tracks rarity
 * counts, detects drift from configurable baseline fractions using relative threshold, computes
 * session summaries (drops/min, drift flags), and supports positional heatmap (ROGUE_LOOT_HEAT_H x
 * W) for drop locations. Includes JSON/CSV export for events and heatmap.
 */

#include "loot_analytics.h"
#include <stdio.h>
#include <string.h>

static struct
{
    RogueLootDropEvent ring[ROGUE_LOOT_ANALYTICS_RING_CAP];
    int head;  /* next write */
    int count; /* <= cap */
    int rarity_counts[5];
    /* Drift baseline */
    float baseline_fracs[5];
    float drift_threshold; /* relative deviation threshold */
    /* Time bounds */
    double first_time;
    double last_time;
    /* Heatmap (Phase 18.5) */
    int heat[ROGUE_LOOT_HEAT_H][ROGUE_LOOT_HEAT_W];
} g_la;

/**
 * @brief Resets the analytics state to initial empty configuration.
 *
 * Clears ring buffer, counts, times, heatmap; sets uniform baseline fractions (0.2 each) and 50%
 * drift threshold.
 */
void rogue_loot_analytics_reset(void)
{
    memset(&g_la, 0, sizeof g_la);
    /* default uniform baseline */
    for (int i = 0; i < 5; i++)
        g_la.baseline_fracs[i] = 0.2f;
    g_la.drift_threshold = 0.5f; /* 50% relative */
}

/**
 * @brief Internal core for recording a loot drop event.
 *
 * Clamps rarity to 0-4, writes to ring buffer (advances head, caps at RING_CAP), increments count
 * and rarity count, updates first/last times.
 *
 * @param def_index Item definition index.
 * @param rarity Rarity (clamped 0-4).
 * @param t_seconds Timestamp in seconds.
 */
static void internal_record_core(int def_index, int rarity, double t_seconds)
{
    if (rarity < 0 || rarity >= 5)
        rarity = 0;
    RogueLootDropEvent* e = &g_la.ring[g_la.head];
    *e = (RogueLootDropEvent){def_index, rarity, t_seconds};
    g_la.head = (g_la.head + 1) % ROGUE_LOOT_ANALYTICS_RING_CAP;
    if (g_la.count < ROGUE_LOOT_ANALYTICS_RING_CAP)
        g_la.count++;
    g_la.rarity_counts[rarity]++;
    if (g_la.count == 1)
        g_la.first_time = t_seconds;
    g_la.last_time = t_seconds;
}

/**
 * @brief Records a loot drop event for analytics.
 *
 * Forwards to internal core for ring buffer and count updates.
 *
 * @param def_index Item definition index.
 * @param rarity Rarity of the drop.
 * @param t_seconds Current timestamp.
 */
void rogue_loot_analytics_record(int def_index, int rarity, double t_seconds)
{
    internal_record_core(def_index, rarity, t_seconds);
}

/**
 * @brief Returns the total number of recorded drop events.
 *
 * @return Current count (<= RING_CAP).
 */
int rogue_loot_analytics_count(void) { return g_la.count; }

/**
 * @brief Retrieves the most recent drop events in reverse chronological order.
 *
 * Copies up to 'max' events from ring tail into out array (oldest first).
 *
 * @param max Maximum number to retrieve (clamped to count).
 * @param out Array to fill with events.
 * @return Number of events copied.
 */
int rogue_loot_analytics_recent(int max, RogueLootDropEvent* out)
{
    if (max <= 0 || !out)
        return 0;
    int n = g_la.count < max ? g_la.count : max;
    for (int i = 0; i < n; i++)
    {
        int idx = (g_la.head - 1 - i);
        if (idx < 0)
            idx += ROGUE_LOOT_ANALYTICS_RING_CAP;
        out[i] = g_la.ring[idx];
    }
    return n;
}

/**
 * @brief Copies current rarity counts into output array.
 *
 * @param out_counts Array of 5 ints to fill (rarity 0-4).
 */
void rogue_loot_analytics_rarity_counts(int out_counts[5])
{
    if (!out_counts)
        return;
    for (int i = 0; i < 5; i++)
        out_counts[i] = g_la.rarity_counts[i];
}

/**
 * @brief Exports basic analytics (count and rarity counts) to JSON string.
 *
 * Format: {"drop_events":int,"rarity_counts":[5 ints]}.
 *
 * @param buf Output buffer.
 * @param cap Buffer capacity.
 * @return 0 on success, -1 invalid params, -2 on write error/truncation.
 */
int rogue_loot_analytics_export_json(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    int rc[5];
    rogue_loot_analytics_rarity_counts(rc);
    int written =
        snprintf(buf, (size_t) cap, "{\"drop_events\":%d,\"rarity_counts\":[%d,%d,%d,%d,%d]}",
                 g_la.count, rc[0], rc[1], rc[2], rc[3], rc[4]);
    if (written < 0 || written >= cap)
        return -2;
    return 0;
}

/**
 * @brief Sets baseline rarity fractions from raw count array.
 *
 * Normalizes non-negative counts to fractions (sum to 1.0), defaults to uniform 0.2 if total <=0 or
 * all invalid.
 *
 * @param counts Array of 5 raw counts for rarities 0-4.
 */
void rogue_loot_analytics_set_baseline_counts(const int counts[5])
{
    if (!counts)
        return;
    int total = 0;
    for (int i = 0; i < 5; i++)
    {
        if (counts[i] < 0)
            continue;
        total += counts[i];
    }
    if (total <= 0)
    {
        for (int i = 0; i < 5; i++)
            g_la.baseline_fracs[i] = 0.2f;
        return;
    }
    for (int i = 0; i < 5; i++)
        g_la.baseline_fracs[i] = counts[i] < 0 ? 0.f : (float) counts[i] / (float) total;
}

/**
 * @brief Sets baseline rarity fractions directly, normalizing to sum=1.0.
 *
 * Clamps negative fracs to 0, normalizes by sum if >0, else defaults to uniform 0.2.
 *
 * @param fracs Array of 5 float fractions for rarities 0-4.
 */
void rogue_loot_analytics_set_baseline_fractions(const float fracs[5])
{
    if (!fracs)
        return;
    float sum = 0.f;
    for (int i = 0; i < 5; i++)
    {
        float f = fracs[i];
        if (f < 0)
            f = 0;
        g_la.baseline_fracs[i] = f;
        sum += f;
    }
    if (sum > 0)
    {
        for (int i = 0; i < 5; i++)
            g_la.baseline_fracs[i] /= sum;
    }
    else
    {
        for (int i = 0; i < 5; i++)
            g_la.baseline_fracs[i] = 0.2f;
    }
}

/**
 * @brief Sets the relative deviation threshold for drift detection.
 *
 * Only sets if >0.
 *
 * @param rel_fraction The threshold fraction (e.g., 0.5 for 50%).
 */
void rogue_loot_analytics_set_drift_threshold(float rel_fraction)
{
    if (rel_fraction > 0.f)
        g_la.drift_threshold = rel_fraction;
}

/**
 * @brief Checks for drift in observed rarity distribution vs baseline.
 *
 * Computes observed fractions, relative diffs, flags if |diff| > threshold. Returns if any drift.
 *
 * @param out_flags Optional array of 5 bools for per-rarity drift flags.
 * @return 1 if any rarity drifted, 0 if no total drops or no drift.
 */
int rogue_loot_analytics_check_drift(int out_flags[5])
{
    if (out_flags)
        for (int i = 0; i < 5; i++)
            out_flags[i] = 0;
    int total = 0;
    for (int i = 0; i < 5; i++)
        total += g_la.rarity_counts[i];
    if (total <= 0)
        return 0;
    int any = 0;
    for (int i = 0; i < 5; i++)
    {
        float expected = g_la.baseline_fracs[i];
        float observed = (float) g_la.rarity_counts[i] / (float) total;
        float diff = expected > 0 ? (observed - expected) / expected : 0.f;
        int drift = expected > 0 && (diff > g_la.drift_threshold || diff < -g_la.drift_threshold);
        if (drift)
            any = 1;
        if (out_flags)
            out_flags[i] = drift;
    }
    return any;
}

/**
 * @brief Fills a session summary struct with current analytics.
 *
 * Includes total drops, rarity counts, duration (last-first time), drops/min, drift check results.
 *
 * @param out Pointer to RogueLootSessionSummary to populate.
 */
void rogue_loot_analytics_session_summary(RogueLootSessionSummary* out)
{
    if (!out)
        return;
    memset(out, 0, sizeof *out);
    out->total_drops = g_la.count;
    for (int i = 0; i < 5; i++)
        out->rarity_counts[i] = g_la.rarity_counts[i];
    if (g_la.count > 1)
    {
        out->duration_seconds = g_la.last_time - g_la.first_time;
        if (out->duration_seconds < 0)
            out->duration_seconds = 0;
    }
    if (out->duration_seconds > 0.01)
        out->drops_per_min = (double) g_la.count / out->duration_seconds * 60.0;
    int flags[5];
    out->drift_any = rogue_loot_analytics_check_drift(flags);
    for (int i = 0; i < 5; i++)
        out->drift_flags[i] = flags[i];
}

/**
 * @brief Records a loot drop with position for heatmap.
 *
 * Calls core record, then increments heatmap cell if coords valid (0 <= x < W, 0 <= y < H).
 *
 * @param def_index Item index.
 * @param rarity Rarity.
 * @param t_seconds Timestamp.
 * @param x X coordinate.
 * @param y Y coordinate.
 */
void rogue_loot_analytics_record_pos(int def_index, int rarity, double t_seconds, int x, int y)
{
    internal_record_core(def_index, rarity, t_seconds);
    if (x >= 0 && x < ROGUE_LOOT_HEAT_W && y >= 0 && y < ROGUE_LOOT_HEAT_H)
    {
        g_la.heat[y][x]++;
    }
}

/**
 * @brief Queries the drop count at a specific heatmap position.
 *
 * @param x X coordinate.
 * @param y Y coordinate.
 * @return Heat value (drops) at (x,y), 0 if out of bounds.
 */
int rogue_loot_analytics_heat_at(int x, int y)
{
    if (x < 0 || x >= ROGUE_LOOT_HEAT_W || y < 0 || y >= ROGUE_LOOT_HEAT_H)
        return 0;
    return g_la.heat[y][x];
}

/**
 * @brief Exports the heatmap to semicolon-separated CSV rows (y-major order).
 *
 * Simple header "x0\n", then each row: heat[y][0];heat[y][1];...heat[y][W-1]\n. Truncates on
 * overflow.
 *
 * @param buf Output buffer.
 * @param cap Buffer capacity.
 * @return 0 on success, -1 invalid params, -2 on write error/truncation.
 */
int rogue_loot_analytics_export_heatmap_csv(char* buf, int cap)
{
    if (!buf || cap <= 0)
        return -1;
    int written = 0;
    int n = snprintf(buf + written, (size_t) (cap - written), "x0");
    if (n < 0)
        return -1;
    written += n; /* simple header stub */
    n = snprintf(buf + written, (size_t) (cap - written), "\n");
    if (n < 0)
        return -1;
    written += n;
    for (int y = 0; y < ROGUE_LOOT_HEAT_H; y++)
    {
        for (int x = 0; x < ROGUE_LOOT_HEAT_W; x++)
        {
            n = snprintf(buf + written, (size_t) (cap - written), "%d%s", g_la.heat[y][x],
                         (x == ROGUE_LOOT_HEAT_W - 1) ? "" : ";");
            if (n < 0 || written + n >= cap)
                return -2;
            written += n;
        }
        n = snprintf(buf + written, (size_t) (cap - written), "\n");
        if (n < 0 || written + n >= cap)
            return -2;
        written += n;
    }
    return 0;
}
