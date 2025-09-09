#include "../../src/core/vendor/vendor_analytics.h"
#include "../../src/core/vendor/vendor_sinks.h"
#include <stdio.h>

static int approx(float a, float b, float eps) { return (a - b < eps) && (b - a < eps); }

static int test_elasticity_stability(void)
{
    /* Build a symmetric dataset: (-10,120), (0,100), (10,80) → slope ~ -2.0 units per 1% */
    rogue_vendor_analytics_reset();
    rogue_vendor_sinks_reset();

    int cat = ROGUE_ITEM_CONSUMABLE;
    rogue_vendor_analytics_record_elasticity_observation(cat, -10.0f, 120);
    rogue_vendor_analytics_record_elasticity_observation(cat, 0.0f, 100);
    rogue_vendor_analytics_record_elasticity_observation(cat, 10.0f, 80);
    float s0 = rogue_vendor_analytics_price_elasticity_slope(cat);
    if (!approx(s0, -2.0f, 0.05f))
        return 10;

    /* Duplicate the dataset multiple times – slope should remain invariant under duplication. */
    for (int i = 0; i < 3; ++i)
    {
        rogue_vendor_analytics_record_elasticity_observation(cat, -10.0f, 120);
        rogue_vendor_analytics_record_elasticity_observation(cat, 0.0f, 100);
        rogue_vendor_analytics_record_elasticity_observation(cat, 10.0f, 80);
    }
    float s1 = rogue_vendor_analytics_price_elasticity_slope(cat);
    if (!approx(s1, s0, 1e-3f))
        return 11;

    /* Degenerate case: all x equal → denom ~ 0 → slope 0 */
    rogue_vendor_analytics_reset();
    rogue_vendor_analytics_record_elasticity_observation(cat, 0.0f, 50);
    rogue_vendor_analytics_record_elasticity_observation(cat, 0.0f, 100);
    rogue_vendor_analytics_record_elasticity_observation(cat, 0.0f, 200);
    float sdeg = rogue_vendor_analytics_price_elasticity_slope(cat);
    if (!approx(sdeg, 0.0f, 1e-6f))
        return 12;

    return 0;
}

static int test_drift_false_positive(void)
{
    rogue_vendor_analytics_reset();
    /* Keep default threshold (25%) or explicitly set */
    rogue_vendor_analytics_set_drift_threshold(0.25f);

    /* Establish baseline anchor at 100 */
    rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_WEAPON, 1, 100, 0.0f, 0);

    /* Feed bounded noise within ±5% around anchor. */
    const int seq[] = {103, 97, 102, 98, 101, 99};
    const int nseq = (int) (sizeof(seq) / sizeof(seq[0]));
    for (int i = 0; i < 200; ++i)
    {
        int price = seq[i % nseq];
        rogue_vendor_analytics_record_vendor_sale(ROGUE_ITEM_WEAPON, 1, price, 0.0f, 0);
    }
    float drift = -1.0f;
    int latched = rogue_vendor_analytics_check_price_drift(&drift);
    if (latched != 0)
        return 20;
    if (drift < 0.0f || drift >= 0.25f)
        return 21;
    /* Be conservative: expect EWMA deviation well below 10% here. */
    if (drift >= 0.10f)
        return 22;

    return 0;
}

int main(void)
{
    int rc = 0;
    if ((rc = test_elasticity_stability()) != 0)
        return rc;
    if ((rc = test_drift_false_positive()) != 0)
        return rc;
    return 0;
}
