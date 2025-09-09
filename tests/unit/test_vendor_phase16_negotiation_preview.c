#include "../../src/core/vendor/vendor_registry.h"
#include "../../src/core/vendor/vendor_ui_negotiation.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_preview_basic()
{
    int ok = rogue_vendor_registry_load_all();
    assert(ok);
    int minp = -1, maxp = -1;
    float prob = -1.0f;
    /* Rule: standard_skill_check -> tags: insight(int), finesse(dex), min_roll=15, 3..8% */
    int rc = rogue_vendor_ui_negotiation_preview("standard_skill_check",
                                                 /* STR DEX INT VIT */ 5, 10, 10, 3, &minp, &maxp,
                                                 &prob);
    assert(rc == 1);
    assert(minp == 3);
    assert(maxp == 8);
    /* avg = (DEX 10 + INT 10)/2 = 10; need=15-10=5 => P=(21-5)/20=16/20=0.8 */
    assert(fabsf(prob - 0.8f) < 0.001f);
}

static void test_preview_edge_low_stats()
{
    int minp = 0, maxp = 0;
    float prob = 0.0f;
    int rc = rogue_vendor_ui_negotiation_preview("standard_skill_check", 0, 0, 0, 0, &minp, &maxp,
                                                 &prob);
    assert(rc == 1);
    /* need=15-0=15 => P=(21-15)/20=6/20=0.3 */
    assert(fabsf(prob - 0.3f) < 0.001f);
}

int main(void)
{
    test_preview_basic();
    test_preview_edge_low_stats();
    printf("OK\n");
    return 0;
}
