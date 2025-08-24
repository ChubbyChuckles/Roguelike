/* Tests for global + category drop rate layer (9.1) */
#include "../../src/core/app/app_state.h"
#include "../../src/core/loot/loot_drop_rates.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/path_utils.h"
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static int fail(const char* m)
{
    printf("FAIL:%s\n", m);
    return 1;
}

/* Count rolls by invoking loot_roll many times; with global scalar 0 expect zero drops; with >1
 * expect inflated average. */
int main(void)
{
    rogue_item_defs_reset();
    if (rogue_item_defs_load_from_cfg("../../assets/test_items.cfg") <= 0)
        return fail("item_defs");
    rogue_loot_tables_reset();
    if (rogue_loot_tables_load_from_cfg("../../assets/test_loot_tables.cfg") <= 0)
        return fail("tables");
    rogue_drop_rates_reset();
    unsigned int seed = 1234u;
    int def_idx[32];
    int qty[32];
    /* Baseline sample */
    int baseline = 0;
    for (int i = 0; i < 50; i++)
    {
        unsigned int s = seed + i;
        baseline += rogue_loot_roll(0, &s, 32, def_idx, qty);
    }
    if (baseline <= 0)
        return fail("baseline");
    /* Zero out with global scalar 0 */
    rogue_drop_rates_set_global(0.0f);
    int zero_sum = 0;
    for (int i = 0; i < 50; i++)
    {
        unsigned int s = seed + i;
        zero_sum += rogue_loot_roll(0, &s, 32, def_idx, qty);
    }
    if (zero_sum != 0)
        return fail("global_zero");
    /* Boost with scalar 2.0 (approx double; allow > baseline) */
    rogue_drop_rates_set_global(2.0f);
    int boosted = 0;
    for (int i = 0; i < 50; i++)
    {
        unsigned int s = seed + i;
        boosted += rogue_loot_roll(0, &s, 32, def_idx, qty);
    }
    if (boosted <= baseline)
        return fail("global_boost");
    /* Category suppression: set weapon category scalar to 0 and confirm some drops removed relative
     * to previous boosted run */
    int weapon_index = rogue_item_def_index("long_sword");
    if (weapon_index >= 0)
    {
        const RogueItemDef* wdef = rogue_item_def_at(weapon_index);
        if (wdef)
        {
            rogue_drop_rates_set_category(wdef->category, 0.0f);
            int after_suppress = 0;
            for (int i = 0; i < 50; i++)
            {
                unsigned int s = seed + i;
                after_suppress += rogue_loot_roll(0, &s, 32, def_idx, qty);
            }
            if (after_suppress >= boosted)
                return fail("category_suppress");
        }
    }
    printf("DROP_RATES_OK baseline=%d boosted=%d\n", baseline, boosted);
    return 0;
}
