/* Test vendor inventory generation (10.1) & price formula monotonicity (10.2) */
#include "../../src/core/loot/loot_drop_rates.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/path_utils.h"
#include "../../src/core/vendor/vendor.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    char pitems[256];
    char ptables[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "VENDOR_FAIL find test_items.cfg\n");
        fflush(stderr);
        return 20;
    }
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        fprintf(stderr, "VENDOR_FAIL find test_loot_tables.cfg\n");
        fflush(stderr);
        return 21;
    }
    rogue_item_defs_reset();
    int items = rogue_item_defs_load_from_cfg(pitems);
    if (items <= 0)
    {
        fprintf(stderr, "VENDOR_FAIL items=%d\n", items);
        fflush(stderr);
        return 22;
    }
    /* Ensure drop rates scalars initialized (otherwise static zeroed array would suppress all drops
     * in Release builds). */
    rogue_drop_rates_reset();
    rogue_loot_tables_reset();
    int tables = rogue_loot_tables_load_from_cfg(ptables);
    if (tables <= 0)
    {
        fprintf(stderr, "VENDOR_FAIL tables=%d\n", tables);
        fflush(stderr);
        return 10;
    }
    int t = rogue_loot_table_index("SKELETON_WARRIOR");
    if (t < 0)
    {
        fprintf(stderr, "VENDOR_FAIL table_index=%d\n", t);
        fflush(stderr);
        return 11;
    }
    rogue_vendor_reset();
    RogueGenerationContext ctx = {
        .enemy_level = 5, .biome_id = 0, .enemy_archetype = 1, .player_luck = 2};
    unsigned int seed = 12345u;
    int gen = rogue_vendor_generate_inventory(t, 8, &ctx, &seed);
    if (gen <= 0 || gen > 8)
    {
        fprintf(stderr, "VENDOR_INVENTORY_FAIL gen=%d\n", gen);
        fflush(stderr);
        return 1;
    }
    if (rogue_vendor_item_count() != gen)
    {
        fprintf(stderr, "VENDOR_COUNT_MISMATCH gen=%d count=%d\n", gen, rogue_vendor_item_count());
        fflush(stderr);
        return 2;
    }
    /* Check price >0 and deterministic per rarity for first item definition index encountered. */
    for (int i = 0; i < rogue_vendor_item_count(); i++)
    {
        const RogueVendorItem* vi = rogue_vendor_get(i);
        if (!vi)
        {
            fprintf(stderr, "VENDOR_FAIL null item index=%d\n", i);
            return 30;
        }
        if (vi->price <= 0)
        {
            fprintf(stderr, "VENDOR_FAIL price index=%d price=%d\n", i, vi->price);
            return 31;
        }
    }
    /* Verify multiplier ladder non-decreasing */
    if (items > 0)
    {
        int def0_index = 0;
        const RogueItemDef* d0 = rogue_item_def_at(def0_index);
        if (d0)
        {
            int last = 0;
            for (int r = 0; r < 5; r++)
            {
                int p = rogue_vendor_price_formula(def0_index, r);
                if (p < last)
                {
                    fprintf(stderr, "VENDOR_FAIL price ladder r=%d p=%d last=%d\n", r, p, last);
                    return 32;
                }
                last = p;
            }
        }
    }
    printf("VENDOR_INVENTORY_OK count=%d\n", gen);
    return 0;
}
