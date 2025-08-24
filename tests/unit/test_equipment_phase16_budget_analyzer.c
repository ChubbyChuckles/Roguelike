#include "../../src/core/equipment/equipment_budget_analyzer.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal mocks: we rely on existing runtime init from rogue_core test harness; if not available we
   include minimal init. This test crafts synthetic item instances with controlled affix weights to
   exercise analyzer buckets and over-budget detection. */

static void spawn_item_with_affixes(int def_index, int rarity, int item_level, int prefix_val,
                                    int suffix_val, int over_budget_adjust)
{
    int idx = rogue_items_spawn(def_index, 1, 0.f, 0.f);
    if (idx < 0)
    {
        printf("spawn failed\n");
        exit(1);
    }
    /* Apply baseline */
    rogue_item_instance_apply_affixes(idx, rarity, -1, 0, -1, 0);
    /* Directly mutate instance for deterministic weights */
    const RogueItemInstance* itc = rogue_item_instance_at(idx);
    RogueItemInstance* it = (RogueItemInstance*) itc; /* safe for internal tests */
    it->item_level = item_level;
    if (prefix_val > 0)
    {
        it->prefix_index = 0;
        it->prefix_value = prefix_val;
    }
    if (suffix_val > 0)
    {
        it->suffix_index = 1;
        it->suffix_value = suffix_val;
    }
    if (over_budget_adjust > 0)
    { /* push values up to exceed cap intentionally */
        it->prefix_value += over_budget_adjust;
    }
}

static void assert_true(int cond, const char* msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        exit(1);
    }
}

int main()
{
    /* Create a few synthetic item defs & affixes if needed. Assumes at least 1 def & affixes exist;
     * otherwise test cannot proceed. */
    /* We'll just spawn using def 0 multiple times; rarity variations create different caps via
     * rogue_budget_max. */
    /* Layout:
       - 2 items at ~20% utilization
       - 2 items at ~60% utilization
       - 1 item at ~95% utilization
       - 1 item slightly over 100% utilization
    */

    rogue_budget_analyzer_reset();

    int base_level = 5; /* cap = 20 + 5*5 + rarity^2*10. For rarity 1 => 20+25+10=55 */
    int cap_r1 = rogue_budget_max(base_level, 1);
    (void) cap_r1;

    /* <25% bucket: target ratio ~0.2 => weight ~0.2 * cap */
    spawn_item_with_affixes(0, 1, base_level, (int) (cap_r1 * 0.10f), 0, 0);
    spawn_item_with_affixes(0, 1, base_level, (int) (cap_r1 * 0.20f), 0, 0);

    /* 50-75% bucket: ~0.6 */
    spawn_item_with_affixes(0, 1, base_level, (int) (cap_r1 * 0.60f), 0, 0);
    spawn_item_with_affixes(0, 1, base_level, (int) (cap_r1 * 0.65f), 0, 0);

    /* 90-100% bucket: ~0.95 */
    spawn_item_with_affixes(0, 1, base_level, (int) (cap_r1 * 0.95f), 0, 0);

    /* >100% bucket: exceed by +5 */
    int over = (int) (cap_r1 * 1.05f);
    spawn_item_with_affixes(0, 1, base_level, over, 0, 0);

    RogueBudgetReport rep;
    rogue_budget_analyzer_run(&rep);

    assert_true(rep.item_count >= 6, "expected at least 6 items analyzed");
    assert_true(rep.bucket_counts[0] >= 1, "expected lt25 bucket non-zero");
    assert_true(rep.bucket_counts[2] >= 1, "expected lt75 bucket non-zero");
    assert_true(rep.bucket_counts[4] >= 1 || rep.bucket_counts[3] >= 1,
                "expected <=100 buckets non-zero");
    assert_true(rep.bucket_counts[5] >= 1, "expected gt100 bucket non-zero");
    assert_true(rep.over_budget_count >= 1, "expected over-budget detection");

    char json[512];
    int wrote = rogue_budget_analyzer_export_json(json, sizeof json);
    assert_true(wrote > 0, "expected json output");
    assert_true(strstr(json, "item_count") != NULL, "json missing item_count");
    assert_true(strstr(json, "gt100") != NULL, "json missing gt100 bucket");

    printf("Phase16.5 budget analyzer report generator OK\n");
    return 0;
}
