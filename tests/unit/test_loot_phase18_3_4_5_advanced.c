#include "../../src/core/loot/loot_analytics.h"
#include <stdio.h>
#include <string.h>

static int expect(int cond, const char* msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    rogue_loot_analytics_reset();
    /* establish custom baseline skewed toward common (60%), then others equal */
    int baseline_counts[5] = {60, 15, 10, 10, 5};
    rogue_loot_analytics_set_baseline_counts(baseline_counts);
    rogue_loot_analytics_set_drift_threshold(0.25f); /* 25% relative */

    /* record 100 events matching baseline roughly -> no drift */
    int pass = 1;
    int i = 0;
    for (; i < 60; i++)
        rogue_loot_analytics_record(i, 0, (double) i);
    for (; i < 75; i++)
        rogue_loot_analytics_record(i, 1, (double) i);
    for (; i < 85; i++)
        rogue_loot_analytics_record(i, 2, (double) i);
    for (; i < 95; i++)
        rogue_loot_analytics_record(i, 3, (double) i);
    for (; i < 100; i++)
        rogue_loot_analytics_record(i, 4, (double) i);
    int flags[5];
    int any = rogue_loot_analytics_check_drift(flags);
    pass &= expect(!any, "no drift expected initial");

    /* Force drift: flood rarity 4 */
    for (int k = 0; k < 50; k++)
        rogue_loot_analytics_record(200 + k, 4, 200.0 + k);
    any = rogue_loot_analytics_check_drift(flags);
    pass &= expect(any, "drift expected");
    pass &= expect(flags[4] == 1, "legendary drift flag");

    /* Session summary */
    RogueLootSessionSummary sum;
    rogue_loot_analytics_session_summary(&sum);
    pass &= expect(sum.total_drops == 150, "total drops 150");
    pass &= expect(sum.drift_any == 1, "summary drift any");
    pass &= expect(sum.rarity_counts[4] == 55, "rarity4 count 55");
    pass &= expect(sum.drops_per_min > 0, "drops/min positive");

    /* Heatmap: record positional drops */
    for (int y = 0; y < ROGUE_LOOT_HEAT_H; y++)
    {
        for (int x = 0; x < ROGUE_LOOT_HEAT_W; x++)
        {
            if ((x + y) % 17 == 0)
                rogue_loot_analytics_record_pos(300 + y * ROGUE_LOOT_HEAT_W + x, (x + y) % 5,
                                                4000.0 + x + y, x, y);
        }
    }
    int heat_checks = 0;
    for (int y = 0; y < ROGUE_LOOT_HEAT_H; y++)
    {
        for (int x = 0; x < ROGUE_LOOT_HEAT_W; x++)
        {
            if ((x + y) % 17 == 0)
            {
                heat_checks += rogue_loot_analytics_heat_at(x, y) == 1;
            }
        }
    }
    pass &= expect(heat_checks > 0, "heat points recorded");
    char csv[4096];
    int csv_rc = rogue_loot_analytics_export_heatmap_csv(csv, sizeof csv);
    pass &= expect(csv_rc == 0, "heatmap csv export");
    pass &= expect(strstr(csv, "\n") != NULL, "csv has newline");

    if (pass)
        printf("OK\n");
    return pass ? 0 : 1;
}
