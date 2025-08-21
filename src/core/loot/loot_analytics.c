#include "core/loot/loot_analytics.h"
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

void rogue_loot_analytics_reset(void)
{
    memset(&g_la, 0, sizeof g_la);
    /* default uniform baseline */
    for (int i = 0; i < 5; i++)
        g_la.baseline_fracs[i] = 0.2f;
    g_la.drift_threshold = 0.5f; /* 50% relative */
}

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

void rogue_loot_analytics_record(int def_index, int rarity, double t_seconds)
{
    internal_record_core(def_index, rarity, t_seconds);
}

int rogue_loot_analytics_count(void) { return g_la.count; }

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

void rogue_loot_analytics_rarity_counts(int out_counts[5])
{
    if (!out_counts)
        return;
    for (int i = 0; i < 5; i++)
        out_counts[i] = g_la.rarity_counts[i];
}

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

void rogue_loot_analytics_set_drift_threshold(float rel_fraction)
{
    if (rel_fraction > 0.f)
        g_la.drift_threshold = rel_fraction;
}

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

void rogue_loot_analytics_record_pos(int def_index, int rarity, double t_seconds, int x, int y)
{
    internal_record_core(def_index, rarity, t_seconds);
    if (x >= 0 && x < ROGUE_LOOT_HEAT_W && y >= 0 && y < ROGUE_LOOT_HEAT_H)
    {
        g_la.heat[y][x]++;
    }
}

int rogue_loot_analytics_heat_at(int x, int y)
{
    if (x < 0 || x >= ROGUE_LOOT_HEAT_W || y < 0 || y >= ROGUE_LOOT_HEAT_H)
        return 0;
    return g_la.heat[y][x];
}

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
