#include "vendor_analytics.h"
#include "vendor_sinks.h"
#include <math.h>
#include <string.h>

static int g_sold_by_cat[ROGUE_ITEM__COUNT];
static int g_sold_by_rarity[5];
static int g_bought_by_cat[ROGUE_ITEM__COUNT];
static int g_bought_by_rarity[5];
static int g_gold_spent_total = 0; /* player -> vendor */
static int g_gold_paid_total = 0;  /* vendor -> player */

/* Elasticity accumulators per category: sums for least squares */
typedef struct
{
    int n;
    double sum_x; /* adjustment_pct */
    double sum_y; /* units */
    double sum_xx;
    double sum_xy;
} RogueElasticityAgg;
static RogueElasticityAgg g_elasticity[ROGUE_ITEM__COUNT];

/* Negotiation */
static int g_nego_attempts = 0;
static int g_nego_success = 0;
static int g_nego_skill_sum_success = 0;
static int g_nego_skill_ct_success = 0;
static int g_nego_skill_sum_failure = 0;
static int g_nego_skill_ct_failure = 0;

/* Price drift EWMA vs baseline anchor */
static double g_price_ewma = 0.0;
static int g_price_count = 0;
static double g_price_anchor = 0.0;  /* first observed price EWMA */
static double g_drift_thresh = 0.25; /* 25% default */
static int g_drift_latched = 0;

void rogue_vendor_analytics_reset(void)
{
    memset(g_sold_by_cat, 0, sizeof g_sold_by_cat);
    memset(g_sold_by_rarity, 0, sizeof g_sold_by_rarity);
    memset(g_bought_by_cat, 0, sizeof g_bought_by_cat);
    memset(g_bought_by_rarity, 0, sizeof g_bought_by_rarity);
    g_gold_spent_total = 0;
    g_gold_paid_total = 0;
    memset(g_elasticity, 0, sizeof g_elasticity);
    g_nego_attempts = 0;
    g_nego_success = 0;
    g_nego_skill_sum_success = g_nego_skill_ct_success = 0;
    g_nego_skill_sum_failure = g_nego_skill_ct_failure = 0;
    g_price_ewma = 0.0;
    g_price_anchor = 0.0;
    g_price_count = 0;
    g_drift_thresh = 0.25;
    g_drift_latched = 0;
}

static void record_price_for_drift(int price)
{
    if (price <= 0)
        return;
    double p = (double) price;
    if (g_price_count == 0)
    {
        g_price_ewma = p;
        g_price_anchor = p;
        g_price_count = 1;
    }
    else
    {
        const double a = 0.05; /* EWMA alpha */
        g_price_ewma = g_price_ewma * (1.0 - a) + p * a;
        g_price_count++;
    }
    double anchor = (g_price_anchor > 0.0) ? g_price_anchor : g_price_ewma;
    if (anchor <= 0.0)
        anchor = 1.0;
    double dev = fabs((g_price_ewma - anchor) / anchor);
    if (dev >= g_drift_thresh)
        g_drift_latched = 1;
}

void rogue_vendor_analytics_record_vendor_sale(int category, int rarity, int price,
                                               float adjustment_pct, unsigned int now_ms)
{
    (void) now_ms;
    if (category >= 0 && category < ROGUE_ITEM__COUNT)
        g_sold_by_cat[category]++;
    if (rarity >= 0 && rarity < 5)
        g_sold_by_rarity[rarity]++;
    if (price > 0)
        g_gold_spent_total += price;
    /* elasticity: units=1 at adjustment_pct */
    if (category >= 0 && category < ROGUE_ITEM__COUNT)
    {
        RogueElasticityAgg* a = &g_elasticity[category];
        double x = (double) adjustment_pct;
        double y = 1.0;
        a->n++;
        a->sum_x += x;
        a->sum_y += y;
        a->sum_xx += x * x;
        a->sum_xy += x * y;
    }
    record_price_for_drift(price);
}

void rogue_vendor_analytics_record_buyback(int category, int rarity, int price, unsigned int now_ms)
{
    (void) now_ms;
    if (category >= 0 && category < ROGUE_ITEM__COUNT)
        g_bought_by_cat[category]++;
    if (rarity >= 0 && rarity < 5)
        g_bought_by_rarity[rarity]++;
    if (price > 0)
        g_gold_paid_total += price;
}

void rogue_vendor_analytics_counts_sold_by_category(int out_counts[ROGUE_ITEM__COUNT])
{
    if (!out_counts)
        return;
    memcpy(out_counts, g_sold_by_cat, sizeof g_sold_by_cat);
}
void rogue_vendor_analytics_counts_sold_by_rarity(int out_counts[5])
{
    if (!out_counts)
        return;
    memcpy(out_counts, g_sold_by_rarity, sizeof g_sold_by_rarity);
}
void rogue_vendor_analytics_counts_bought_by_category(int out_counts[ROGUE_ITEM__COUNT])
{
    if (!out_counts)
        return;
    memcpy(out_counts, g_bought_by_cat, sizeof g_bought_by_cat);
}
void rogue_vendor_analytics_counts_bought_by_rarity(int out_counts[5])
{
    if (!out_counts)
        return;
    memcpy(out_counts, g_bought_by_rarity, sizeof g_bought_by_rarity);
}

int rogue_vendor_analytics_gold_spent_total(void) { return g_gold_spent_total; }
int rogue_vendor_analytics_gold_paid_total(void) { return g_gold_paid_total; }

float rogue_vendor_analytics_sink_coverage_ratio(void)
{
    int spent = g_gold_spent_total;
    if (spent <= 0)
        return 0.0f;
    int sinks = rogue_vendor_sinks_grand_total();
    if (sinks < 0)
        sinks = 0;
    return (float) ((double) sinks / (double) spent);
}

void rogue_vendor_analytics_record_elasticity_observation(int category, float adjustment_pct,
                                                          int units_sold)
{
    if (category < 0 || category >= ROGUE_ITEM__COUNT || units_sold <= 0)
        return;
    RogueElasticityAgg* a = &g_elasticity[category];
    double x = (double) adjustment_pct;
    double y = (double) units_sold;
    a->n++;
    a->sum_x += x;
    a->sum_y += y;
    a->sum_xx += x * x;
    a->sum_xy += x * y;
}

float rogue_vendor_analytics_price_elasticity_slope(int category)
{
    if (category < 0 || category >= ROGUE_ITEM__COUNT)
        return 0.0f;
    RogueElasticityAgg* a = &g_elasticity[category];
    if (a->n < 2)
        return 0.0f;
    double n = (double) a->n;
    double denom = (n * a->sum_xx - a->sum_x * a->sum_x);
    if (fabs(denom) < 1e-9)
        return 0.0f;
    double slope = (n * a->sum_xy - a->sum_x * a->sum_y) / denom;
    return (float) slope;
}

void rogue_vendor_analytics_record_negotiation(int success_flag, int discount_pct,
                                               int avg_skill_score)
{
    (void) discount_pct; /* reserved for future histograms */
    g_nego_attempts++;
    if (success_flag)
    {
        g_nego_success++;
        g_nego_skill_sum_success += avg_skill_score;
        g_nego_skill_ct_success++;
    }
    else
    {
        g_nego_skill_sum_failure += avg_skill_score;
        g_nego_skill_ct_failure++;
    }
}

float rogue_vendor_analytics_negotiation_success_rate(void)
{
    if (g_nego_attempts <= 0)
        return 0.0f;
    return (float) ((double) g_nego_success / (double) g_nego_attempts);
}
float rogue_vendor_analytics_negotiation_avg_skill_success(void)
{
    if (g_nego_skill_ct_success <= 0)
        return 0.0f;
    return (float) ((double) g_nego_skill_sum_success / (double) g_nego_skill_ct_success);
}
float rogue_vendor_analytics_negotiation_avg_skill_failure(void)
{
    if (g_nego_skill_ct_failure <= 0)
        return 0.0f;
    return (float) ((double) g_nego_skill_sum_failure / (double) g_nego_skill_ct_failure);
}

void rogue_vendor_analytics_set_drift_threshold(float rel_fraction)
{
    if (rel_fraction <= 0.0f)
        rel_fraction = 0.10f;
    if (rel_fraction > 1.0f)
        rel_fraction = 1.0f;
    g_drift_thresh = rel_fraction;
}

int rogue_vendor_analytics_check_price_drift(float* out_abs_drift)
{
    if (g_price_count <= 1)
    {
        if (out_abs_drift)
            *out_abs_drift = 0.0f;
        return 0;
    }
    double anchor = (g_price_anchor > 0.0) ? g_price_anchor : g_price_ewma;
    if (anchor <= 0.0)
        anchor = 1.0;
    double dev = fabs((g_price_ewma - anchor) / anchor);
    if (out_abs_drift)
        *out_abs_drift = (float) dev;
    return g_drift_latched ? 1 : 0;
}
