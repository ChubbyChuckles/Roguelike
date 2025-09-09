#include "../../src/core/vendor/vendor_pricing.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_tooltip_format_basic(void)
{
    RogueVendorPriceBreakdown br;
    int p = rogue_vendor_compose_price_testonly(
        100, /* base */ 1.0f, /* cond */ 1.20f, /* policy */ 0.90f, /* rep */ 0.95f, 1.05f,
        /* demand */ 1.02f, /* scarcity */ 1.00f, /* exploit */ 1.00f, /* sec */
        1.00f, /* global */ 1.00f, /* biome */ &br);
    assert(p == br.final_price);
    char buf[256];
    int n = rogue_vendor_format_price_tooltip(&br, buf, (int) sizeof buf);
    assert(n > 0);
    /* Snapshot-style contains lines and key labels */
    assert(strstr(buf, "Base:") != NULL);
    assert(strstr(buf, "Policy:") != NULL);
    assert(strstr(buf, "Final:") != NULL);
}

int main(void)
{
    test_tooltip_format_basic();
    printf("VENDOR_PHASE16_TOOLTIP_OK\n");
    return 0;
}
