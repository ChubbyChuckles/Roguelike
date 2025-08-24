/* Vendor System Phase 1 (1.1-1.3) registry & parsing tests */
#include "../../src/core/vendor/vendor_registry.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    if (!rogue_vendor_registry_load_all())
    {
        fprintf(stderr, "VENDOR_P1_FAIL load_all\n");
        return 5;
    }
    if (rogue_vendor_def_count() <= 0)
    {
        fprintf(stderr, "VENDOR_P1_FAIL vendors count\n");
        return 6;
    }
    if (rogue_price_policy_count() <= 0)
    {
        fprintf(stderr, "VENDOR_P1_FAIL policies count\n");
        return 7;
    }
    if (rogue_rep_tier_count() <= 0)
    {
        fprintf(stderr, "VENDOR_P1_FAIL rep tiers count\n");
        return 8;
    }
    /* JSON migration: expect 'blacksmith_standard' from vendors.json */
    const RogueVendorDef* blacksmith = rogue_vendor_def_find("blacksmith_standard");
    if (!blacksmith)
    {
        fprintf(stderr, "VENDOR_P1_FAIL find blacksmith_standard\n");
        return 9;
    }
    if (blacksmith->price_policy_index < 0)
    {
        fprintf(stderr, "VENDOR_P1_FAIL policy index resolve\n");
        return 10;
    }
    const RoguePricePolicy* pol = rogue_price_policy_at(blacksmith->price_policy_index);
    if (!pol)
    {
        fprintf(stderr, "VENDOR_P1_FAIL policy deref\n");
        return 11;
    }
    if (pol->base_buy_margin <= 0 || pol->base_sell_margin <= 0)
    {
        fprintf(stderr, "VENDOR_P1_FAIL policy margins\n");
        return 12;
    }
    /* Reputation tier ordering by rep_min ascending */
    int last = -1;
    for (int i = 0; i < rogue_rep_tier_count(); i++)
    {
        const RogueRepTier* rt = rogue_rep_tier_at(i);
        if (rt->rep_min < last)
        {
            fprintf(stderr, "VENDOR_P1_FAIL rep tier order\n");
            return 13;
        }
        last = rt->rep_min;
    }
    /* Negotiation rules now part of Phase 1.4 */
    if (rogue_negotiation_rule_count() <= 0)
    {
        fprintf(stderr, "VENDOR_P1_FAIL negotiation count\n");
        return 14;
    }
    printf("VENDOR_PHASE1_REGISTRY_OK vendors=%d policies=%d rep=%d nego=%d\n",
           rogue_vendor_def_count(), rogue_price_policy_count(), rogue_rep_tier_count(),
           rogue_negotiation_rule_count());
    return 0;
}
