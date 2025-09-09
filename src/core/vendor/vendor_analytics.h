/* Vendor Analytics & Telemetry (Phase 14)
 * Implements:
 *  - 14.1 Purchase frequency by category & rarity
 *  - 14.2 Gold sink effectiveness (inflow vs outflow ratio)
 *  - 14.3 Price elasticity (quantity vs price adjustments) – simple linear fit per category
 *  - 14.4 Negotiation success distribution & skill correlation (averages)
 *  - 14.5 Inflation/price drift monitor & alerting (EWMA vs baseline anchor)
 */
#ifndef ROGUE_VENDOR_ANALYTICS_H
#define ROGUE_VENDOR_ANALYTICS_H

#include "../loot/loot_item_defs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void rogue_vendor_analytics_reset(void);

    /* 14.1 + price drift feed (14.5): record that vendor SOLD an item to player. */
    void rogue_vendor_analytics_record_vendor_sale(int category, int rarity, int price,
                                                   float adjustment_pct, unsigned int now_ms);
    /* 14.1: record that vendor BOUGHT an item back from player. */
    void rogue_vendor_analytics_record_buyback(int category, int rarity, int price,
                                               unsigned int now_ms);

    /* 14.1: queries */
    void rogue_vendor_analytics_counts_sold_by_category(int out_counts[ROGUE_ITEM__COUNT]);
    void rogue_vendor_analytics_counts_sold_by_rarity(int out_counts[5]);
    void rogue_vendor_analytics_counts_bought_by_category(int out_counts[ROGUE_ITEM__COUNT]);
    void rogue_vendor_analytics_counts_bought_by_rarity(int out_counts[5]);

    /* Totals for gold flow (14.2) */
    int rogue_vendor_analytics_gold_spent_total(void); /* player -> vendor (purchases) */
    int rogue_vendor_analytics_gold_paid_total(void);  /* vendor -> player (buybacks) */
    /* 14.2: sink coverage ratio = sinks_grand_total / gold_spent_total (0 if denom<=0). */
    float rogue_vendor_analytics_sink_coverage_ratio(void);

    /* 14.3: Elasticity observations (optional explicit). If sales are recorded via
       record_vendor_sale with adjustment_pct, these sums are also updated with units=1. */
    void rogue_vendor_analytics_record_elasticity_observation(int category, float adjustment_pct,
                                                              int units_sold);
    /* Returns simple slope dy/dx from least-squares fit of units vs adjustment_pct for category. */
    float rogue_vendor_analytics_price_elasticity_slope(int category);

    /* 14.4: Negotiation stats (record attempts; avg_skill ~ average of relevant skills used). */
    void rogue_vendor_analytics_record_negotiation(int success_flag, int discount_pct,
                                                   int avg_skill_score);
    float rogue_vendor_analytics_negotiation_success_rate(void); /* successes/attempts */
    float rogue_vendor_analytics_negotiation_avg_skill_success(void);
    float rogue_vendor_analytics_negotiation_avg_skill_failure(void);

    /* 14.5: Price drift monitor (EWMA vs baseline anchor). Threshold default 0.25 (25%). */
    void rogue_vendor_analytics_set_drift_threshold(float rel_fraction);
    /* Returns 1 if |drift| >= threshold at any point since reset (latched). Optionally outputs
       current absolute drift fraction (>=0). */
    int rogue_vendor_analytics_check_price_drift(float* out_abs_drift);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_VENDOR_ANALYTICS_H */
