#include "core/crafting/crafting_analytics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void simulate()
{
    rogue_craft_analytics_reset();
    /* Simulate 40 node harvests with 5 rare */
    for (int i = 0; i < 40; i++)
    {
        rogue_craft_analytics_on_node_harvest(0, 0, 1, (i % 8) == 0, (unsigned int) (i * 1000));
    }
    /* Simulate 30 crafts with rising quality */
    for (int c = 0; c < 30; c++)
    {
        int q = (c * 3) % 101; /* spread */
        rogue_craft_analytics_on_craft(0, q, (c % 5) != 0);
    }
    /* Enhancement attempts expected risk 0.3 actual failure ~ observed from pattern */
    for (int e = 0; e < 50; e++)
    {
        rogue_craft_analytics_on_enhancement(0.30f, (e % 4) != 0); /* fail every 4th -> 25% */
    }
    /* Material flow */
    for (int m = 0; m < 10; m++)
    {
        rogue_craft_analytics_material_acquire(m, 100 + m);
        rogue_craft_analytics_material_consume(m, 50 + m);
    }
}

int main()
{
    simulate();
    float rrate = rogue_craft_analytics_rare_proc_rate();
    assert(rrate > 0.05f && rrate < 0.40f);
    float var = rogue_craft_analytics_enhance_risk_variance();
    assert(var < 0.1f && var > -0.1f); /* expected ~ -0.05 */
    int drift0 = rogue_craft_analytics_check_quality_drift();
    /* Our quality distribution is broad but average should remain mid-ish -> no drift */
    assert(drift0 == 0);
    char buf[4096];
    int w = rogue_craft_analytics_export_json(buf, sizeof buf);
    assert(w > 0);
    /* Force drift by biasing high quality */
    for (int i = 0; i < 60; i++)
    {
        rogue_craft_analytics_on_craft(0, 95, 1);
    }
    int drift1 = rogue_craft_analytics_check_quality_drift();
    assert(drift1 == 1);
    printf("CRAFT_P11_OK analytics\n");
    return 0;
}
