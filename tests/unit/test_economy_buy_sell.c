#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/vendor/economy.h"
#include "../../src/core/vendor/vendor.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>

int main(void)
{
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "ECON_FAIL find items\n");
        return 10;
    }
    rogue_item_defs_reset();
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        fprintf(stderr, "ECON_FAIL load items\n");
        return 11;
    }
    int def = rogue_item_def_index("epic_axe");
    if (def < 0)
    {
        fprintf(stderr, "ECON_FAIL def epic_axe\n");
        return 12;
    }
    RogueVendorItem v;
    v.def_index = def;
    v.rarity = 3;
    v.price = rogue_vendor_price_formula(def, 3);
    rogue_econ_reset();
    rogue_econ_add_gold(100000);   /* large */
    rogue_econ_set_reputation(50); /* 50% of discount scale -> 0.9 cost */
    int buy_price = rogue_econ_buy_price(&v);
    int sell_value = rogue_econ_sell_value(&v);
    if (sell_value < v.price / 10 || sell_value > (v.price * 70) / 100)
    {
        fprintf(stderr, "ECON_FAIL sell bounds %d base=%d\n", sell_value, v.price);
        return 13;
    }
    if (rogue_econ_try_buy(&v) != 0)
    {
        fprintf(stderr, "ECON_FAIL buy op\n");
        return 14;
    }
    if (rogue_econ_gold() <= 0)
    {
        fprintf(stderr, "ECON_FAIL gold after buy\n");
        return 15;
    }
    int before = rogue_econ_gold();
    int credit = rogue_econ_sell(&v);
    if (credit != sell_value)
    {
        fprintf(stderr, "ECON_FAIL credit mismatch %d %d\n", credit, sell_value);
        return 16;
    }
    if (rogue_econ_gold() != before + sell_value)
    {
        fprintf(stderr, "ECON_FAIL gold not added\n");
        return 17;
    }
    printf("ECON_OK buy=%d sell=%d\n", buy_price, sell_value);
    return 0;
}
