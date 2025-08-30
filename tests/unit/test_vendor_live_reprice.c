#include "../../src/core/loot/item_debug.h"
#include "../../src/core/loot/loot_drop_rates.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/vendor/vendor.h"
#include "../../src/util/path_utils.h"
#include <assert.h>
#include <stdio.h>

/* Phase 10.3: Verify vendor inventory reprices live when item defs change */
int main(void)
{
    char pitems[256];
    char ptables[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL find test_items.cfg\n");
        return 20;
    }
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL find test_loot_tables.cfg\n");
        return 21;
    }
    rogue_item_defs_reset();
    int items = rogue_item_defs_load_from_cfg(pitems);
    if (items <= 0)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL items=%d\n", items);
        return 22;
    }
    rogue_drop_rates_reset();
    rogue_loot_tables_reset();
    int tables = rogue_loot_tables_load_from_cfg(ptables);
    if (tables <= 0)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL tables=%d\n", tables);
        return 10;
    }
    int t = rogue_loot_table_index("SKELETON_WARRIOR");
    if (t < 0)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL table index\n");
        return 11;
    }
    rogue_vendor_reset();
    RogueGenerationContext ctx = {
        .enemy_level = 5, .biome_id = 0, .enemy_archetype = 1, .player_luck = 2};
    unsigned int seed = 98765u;
    int gen = rogue_vendor_generate_inventory(t, 8, &ctx, &seed);
    if (gen <= 0)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL gen=%d\n", gen);
        return 1;
    }
    /* Take first slot; change base_value of its def and ensure price changes accordingly */
    const RogueVendorItem* vi0 = rogue_vendor_get(0);
    if (!vi0)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL no slot0\n");
        return 2;
    }
    int def_index = vi0->def_index;
    int rarity = vi0->rarity;
    int before = vi0->price;
    /* Bump base_value substantially to force price increase */
    const RogueItemDef* d = rogue_item_debug_get(def_index);
    if (!d)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL no def for slot0\n");
        return 3;
    }
    int new_base = d->base_value + 1000;
    (void) rogue_item_debug_set_int(def_index, "base_value", new_base);
    const RogueVendorItem* vi0_after = rogue_vendor_get(0);
    if (!vi0_after)
    {
        fprintf(stderr, "LIVE_REPRICE_FAIL slot0 after null\n");
        return 4;
    }
    int after = vi0_after->price;
    if (after <= before)
    {
        fprintf(stderr,
                "LIVE_REPRICE_FAIL price did not increase: before=%d after=%d (rarity=%d)\n",
                before, after, rarity);
        return 5;
    }
    printf("LIVE_REPRICE_OK before=%d after=%d\n", before, after);
    return 0;
}
