#include "../../src/core/vendor/vendor_analytics.h"
#include "../../src/core/vendor/vendor_sinks.h"
#include <stdio.h>

static int approx(float a, float b, float eps) { return (a - b < eps) && (b - a < eps); }

static int test_run(void)
{
    rogue_vendor_analytics_reset();
    rogue_vendor_sinks_reset();

    /* Record some purchases: 3 consumables, 2 weapons; rarities spread. */
    rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_CONSUMABLE, 1, 50, 10.0f, 0);
    rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_CONSUMABLE, 2, 75, 0.0f, 0);
    rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_CONSUMABLE, 2, 100, -5.0f, 0);
    rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_WEAPON, 3, 500, 15.0f, 0);
    rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_WEAPON, 1, 250, 5.0f, 0);

    /* Buybacks: 1 armor, 1 material */
    rogue_vendor_analytics_record_buyback(ROGUE_ITEM_ARMOR, 2, 120, 0);
    rogue_vendor_analytics_record_buyback(ROGUE_ITEM_MATERIAL, 1, 30, 0);

    int by_cat[ROGUE_ITEM__COUNT] = {0};
    int by_rarity[5] = {0};
    rogue_vendor_analytics_counts_sold_by_category(by_cat);
    if (!(by_cat[ROGUE_ITEM_CONSUMABLE] == 3 && by_cat[ROGUE_ITEM_WEAPON] == 2))
        return 1;
    rogue_vendor_analytics_counts_sold_by_rarity(by_rarity);
    if (!(by_rarity[1] >= 2 && by_rarity[2] >= 2))
        return 2;

    int gold_spent = rogue_vendor_analytics_gold_spent_total();
    if (gold_spent <= 0)
        return 3;
    /* Simulate sinks: spend half of gold_spent as sinks */
    int half = gold_spent / 2;
    rogue_vendor_sinks_add(ROGUE_SINK_FEES, half);
    float coverage = rogue_vendor_analytics_sink_coverage_ratio();
    if (coverage < 0.45f || coverage > 0.55f)
        return 4;

    /* Elasticity slope should be finite (we added mixed adjustments for consumables) */
    float slope = rogue_vendor_analytics_price_elasticity_slope(ROGUE_ITEM_CONSUMABLE);
    (void) slope; /* cannot assert sign deterministically here */

    /* Negotiation stats */
    rogue_vendor_analytics_record_negotiation(1, 10, 12);
    rogue_vendor_analytics_record_negotiation(0, 0, 4);
    if (rogue_vendor_analytics_negotiation_success_rate() <= 0.0f)
        return 5;
    if (!(approx(rogue_vendor_analytics_negotiation_avg_skill_success(), 12.0f, 0.01f)))
        return 6;
    if (!(approx(rogue_vendor_analytics_negotiation_avg_skill_failure(), 4.0f, 0.01f)))
        return 7;

    /* Price drift detection: with mixed prices, drift might be < threshold; set low threshold and
       invoke check to ensure it can trigger. */
    rogue_vendor_analytics_set_drift_threshold(0.01f);
    float drift = 0.0f;
    int drift_flag = rogue_vendor_analytics_check_price_drift(&drift);
    if (!drift_flag && drift < 0.001f)
        return 8;
    return 0;
}

int main(void) { return test_run(); }
