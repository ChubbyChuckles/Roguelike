#include "vendor_adaptive.h"
#include <math.h>
#include <string.h>

#ifndef ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX
#define ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX 16
#endif

/* Purchase profile: we track counts of purchases (vendor->player) and sales (player->vendor) per
 * category. */
static unsigned int g_purchase_counts[ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX];
static unsigned int g_sale_counts[ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX];
/* Recent purchase timestamps ring (for batch gating & exploit detection). */
#ifndef ROGUE_VENDOR_ADAPTIVE_RECENT_CAP
#define ROGUE_VENDOR_ADAPTIVE_RECENT_CAP 64
#endif
static unsigned int g_recent_purchase_ts[ROGUE_VENDOR_ADAPTIVE_RECENT_CAP];
static int g_recent_head = 0;
static int g_recent_count = 0;

/* Flip detection: we maintain a short horizon EWMA of (purchases - sales). Rapid buy->sell (flip)
 * spikes sale count after purchase; detection metric uses correlation of close timestamps. For
 * simplicity we count occurrences where a sale occurs within FLIP_WINDOW_MS of a purchase. */
static unsigned int g_recent_purchase_sale_pairs = 0; /* decays over time */

/* Internal clock last observed (monotonic assumption for decay). */
static unsigned int g_last_ts = 0;

static void decay_pairs(unsigned int now_ms)
{
    if (now_ms <= g_last_ts)
        return;
    unsigned int dt = now_ms - g_last_ts;
    g_last_ts = now_ms; /* simple exponential decay every 5s half-life */
    /* half life 5000ms -> decay factor = 0.5^(dt/5000) approximate using exp2 */
    double factor = pow(0.5, (double) dt / 5000.0);
    double v = (double) g_recent_purchase_sale_pairs * factor;
    if (v < 0.5)
        v = 0.0;
    g_recent_purchase_sale_pairs = (unsigned int) floor(v + 0.5);
}

void rogue_vendor_adaptive_reset(void)
{
    memset(g_purchase_counts, 0, sizeof g_purchase_counts);
    memset(g_sale_counts, 0, sizeof g_sale_counts);
    memset(g_recent_purchase_ts, 0, sizeof g_recent_purchase_ts);
    g_recent_head = 0;
    g_recent_count = 0;
    g_recent_purchase_sale_pairs = 0;
    g_last_ts = 0;
}

void rogue_vendor_adaptive_record_player_purchase(int category, unsigned int timestamp_ms)
{
    if (category >= 0 && category < ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX)
        g_purchase_counts[category]++;
    decay_pairs(timestamp_ms);
    g_recent_purchase_ts[g_recent_head] = timestamp_ms;
    g_recent_head = (g_recent_head + 1) % ROGUE_VENDOR_ADAPTIVE_RECENT_CAP;
    if (g_recent_count < ROGUE_VENDOR_ADAPTIVE_RECENT_CAP)
        g_recent_count++;
}

void rogue_vendor_adaptive_record_player_sale(int category, unsigned int timestamp_ms)
{
    if (category >= 0 && category < ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX)
        g_sale_counts[category]++;
    decay_pairs(timestamp_ms);                  /* detect flip: sale close to any recent purchase */
    const unsigned int FLIP_WINDOW_MS = 15000u; /* 15s */
    for (int i = 0; i < g_recent_count; i++)
    {
        unsigned int ts = g_recent_purchase_ts[i];
        if (timestamp_ms >= ts && timestamp_ms - ts <= FLIP_WINDOW_MS)
        {
            g_recent_purchase_sale_pairs++;
            break;
        }
    }
}

float rogue_vendor_adaptive_category_weight_scalar(int category)
{
    if (category < 0 || category >= ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX)
        return 1.0f;
    unsigned int p = g_purchase_counts[category];
    unsigned int s = g_sale_counts[category]; /* We consider under‑purchased categories those with
                                                 low p relative to max. */
    unsigned int max_p = 1;
    for (int i = 0; i < ROGUE_VENDOR_ADAPTIVE_CATEGORY_MAX; i++)
    {
        if (g_purchase_counts[i] > max_p)
            max_p = g_purchase_counts[i];
    }
    /* Under-purchase score: 1 - (p/max_p). If max_p==0 -> treat all equal (score 0). */
    float score = 0.0f;
    if (max_p > 0)
        score = 1.0f - (float) p /
                           (float) max_p; /* score in [0,1] with higher meaning more boost needed */
    /* Map score via smoothstep into boost range [1.0, 1.15]; apply slight penalty if net sales
     * exceed purchases heavily (avoid boosting purely because of ignoring category when it's just
     * sold often) */
    float net = (float) p - (float) s;
    if (net < 0)
        score *= 0.6f;
    if (score < 0.0f)
        score = 0.0f;
    if (score > 1.0f)
        score = 1.0f;
    float scalar = 1.0f + 0.15f * (score * score * (3 - 2 * score)); /* smoothstep */
    if (scalar < 0.85f)
        scalar = 0.85f;
    if (scalar > 1.15f)
        scalar = 1.15f;
    return scalar;
}

void rogue_vendor_adaptive_apply_category_weights(int* out_weights, const int* base_weights,
                                                  int count)
{
    if (!out_weights || !base_weights || count <= 0)
        return;
    for (int i = 0; i < count; i++)
    {
        int bw = base_weights[i];
        if (bw < 0)
            bw = 0;
        float sc = rogue_vendor_adaptive_category_weight_scalar(i);
        int adj = (int) ((float) bw * sc);
        if (adj < 0)
            adj = 0;
        out_weights[i] = adj;
    }
}

float rogue_vendor_adaptive_exploit_scalar(void)
{ /* Convert flip pair metric into price increase up to +10% */
    /* Activation threshold: >3 pairs within recent window -> scale rises, capped at +10% */
    unsigned int pairs = g_recent_purchase_sale_pairs;
    if (pairs == 0)
        return 1.0f;
    if (pairs >= 10)
        pairs = 10;
    float inc = (float) pairs / 100.0f; /* pairs=10 -> +0.10 */
    if (inc > 0.10f)
        inc = 0.10f;
    return 1.0f + inc;
}

/* Purchase cooldown: if more than MAX_BATCH purchases inside WINDOW triggers cooldown. */
static unsigned int g_cooldown_until_ms = 0;

uint32_t rogue_vendor_adaptive_purchase_cooldown_remaining(uint32_t now_ms)
{
    if (now_ms >= g_cooldown_until_ms)
        return 0;
    return g_cooldown_until_ms - now_ms;
}

int rogue_vendor_adaptive_can_purchase(uint32_t now_ms)
{ /* Sliding window on recent purchases (past 10s). If >8 purchases, apply cooldown 5s. */
    const unsigned int WINDOW_MS = 10000u;
    const int MAX_BATCH = 8;
    const unsigned int COOLDOWN_MS = 5000u;
    if (now_ms < g_cooldown_until_ms)
        return 0;
    int recent = 0;
    for (int i = 0; i < g_recent_count; i++)
    {
        unsigned int ts = g_recent_purchase_ts[i];
        if (now_ms >= ts && now_ms - ts <= WINDOW_MS)
            recent++;
    }
    if (recent > MAX_BATCH)
    {
        g_cooldown_until_ms = now_ms + COOLDOWN_MS;
        return 0;
    }
    return 1;
}
