#include "../../src/core/vendor/vendor_ui_filters.h"
#include <assert.h>
#include <stdio.h>

static void test_filters_roundtrip(void)
{
    rogue_vendor_ui_reset_filters();
    RogueVendorUiFilters f = rogue_vendor_ui_get_filters();
    /* Defaults */
    assert(f.min_rarity == 0 && f.max_rarity == 4);
    /* Change and persist */
    f.min_rarity = 2;
    f.max_rarity = 3;
    f.category_mask = 0x3;
    f.stat_min = 10;
    f.stat_max = 20;
    rogue_vendor_ui_set_filters(&f);
    RogueVendorUiFilters g = rogue_vendor_ui_get_filters();
    assert(g.min_rarity == 2 && g.max_rarity == 3);
    assert(g.category_mask == 0x3);
    assert(g.stat_min == 10 && g.stat_max == 20);
    /* Clamp check: min>max */
    g.min_rarity = 5;
    g.max_rarity = 1; /* invalid */
    rogue_vendor_ui_set_filters(&g);
    RogueVendorUiFilters h = rogue_vendor_ui_get_filters();
    assert(h.min_rarity == h.max_rarity); /* clamped to max */
}

int main(void)
{
    test_filters_roundtrip();
    printf("VENDOR_PHASE16_FILTERS_OK\n");
    return 0;
}
