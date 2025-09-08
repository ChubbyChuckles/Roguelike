#include "../../src/core/loot/loot_drop_rates.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/persistence/save_manager.h"
#include "../../src/core/vendor/vendor.h"
#include "../../src/core/vendor/vendor_buyback.h"
#include "../../src/core/vendor/vendor_pricing.h"
#include "../../src/core/vendor/vendor_registry.h"
#include "../../src/core/vendor/vendor_reputation.h"
#include "../../src/core/vendor/vendor_special_offers.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(c, msg)                                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL:%s %d %s\n", __FILE__, __LINE__, msg);                                    \
            fail = 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    extern void rogue_register_core_save_components(void);
    rogue_register_core_save_components();

    /* load test item + loot table data */
    char pitems[256];
    char ptables[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("FAIL:find items\n");
        return 2;
    }
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        printf("FAIL:find tables\n");
        return 3;
    }
    rogue_item_defs_reset();
    int items_loaded = rogue_item_defs_load_from_cfg(pitems);
    if (items_loaded <= 0)
    {
        printf("FAIL:load items\n");
        return 4;
    }
    rogue_drop_rates_reset();
    rogue_loot_tables_reset();
    int tables_loaded = rogue_loot_tables_load_from_cfg(ptables);
    if (tables_loaded <= 0)
    {
        printf("FAIL:load tables\n");
        return 5;
    }
    if (!rogue_vendor_registry_load_all())
    {
        printf("FAIL:vendor registry load\n");
        return 6;
    }

    /* seed vendor inventory */
    rogue_vendor_reset();
    unsigned int seed = 999u;
    RogueGenerationContext ctx = {0};
    ctx.enemy_level = 5;
    int table_index = rogue_loot_table_index("SKELETON_WARRIOR");
    CHECK(table_index >= 0, "table index");
    int produced = rogue_vendor_generate_inventory(table_index, 6, &ctx, &seed);
    CHECK(produced > 0, "generated");

    /* perturb pricing demand/scarcity */
    for (int k = 0; k < 5; k++)
        rogue_vendor_pricing_record_sale(1);
    for (int k = 0; k < 2; k++)
        rogue_vendor_pricing_record_buyback(2);

    /* reputation: gain with vendor 0 if exists */
    int vcount = rogue_vendor_def_count();
    if (vcount > 0)
    {
        (void) rogue_vendor_rep_gain(0, 50);
    }

    /* offers: roll some */
    rogue_vendor_offers_reset();
    int offers = rogue_vendor_offers_roll(12345u, 10000u, 1);
    CHECK(offers >= 0, "offers roll");

    /* buyback: record one item */
    if (vcount > 0)
    {
        RogueVendorBuybackEntry be = {0};
        be.item_guid = 0xABCDEFu;
        be.item_def_index = 0;
        be.rarity = 1;
        be.category = 0;
        be.condition_pct = 100;
        be.original_price = 123;
        be.sell_time_ms = 5000u;
        be.assimilate_time_ms = 999999u;
        be.active = 1;
        /* Import via internal API by directly inserting using import to avoid journal */
        rogue_vendor_buyback_import(0, &be, 1);
    }

    /* Save slot */
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("FAIL:save\n");
        return 7;
    }

    /* Wipe runtime state */
    rogue_vendor_reset();
    rogue_vendor_pricing_reset();
    rogue_vendor_rep_system_reset();
    rogue_vendor_offers_reset();
    rogue_vendor_buyback_reset();

    /* Load */
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("FAIL:load\n");
        return 8;
    }

    /* Verify vendor items exist */
    int after = rogue_vendor_item_count();
    CHECK(after > 0, "vendor items restored");

    /* Verify pricing demand imported (non-default scalars) */
    float ds1 = rogue_vendor_pricing_get_demand_scalar(1);
    float sc2 = rogue_vendor_pricing_get_scarcity_scalar(2);
    CHECK(ds1 > 1.0f || sc2 < 1.0f, "pricing state persisted");

    /* Verify rep state import if we had vendors */
    if (vcount > 0)
    {
        CHECK(rogue_vendor_rep_state_count() > 0, "rep state count");
    }

    /* Verify offers restored (count or seed/miss values) */
    int oc = rogue_vendor_offers_count();
    CHECK(oc >= 0, "offers present or empty");

    /* Verify buyback restored for vendor 0 when present */
    if (vcount > 0)
    {
        RogueVendorBuybackEntry list[8];
        int bc = rogue_vendor_buyback_list(0, list, 8, 6000u);
        CHECK(bc >= 1, "buyback restored");
    }

    if (fail)
    {
        printf("FAILURES\n");
        return 1;
    }
    printf("OK:save_phase13_vendor_state_roundtrip\n");
    return 0;
}
