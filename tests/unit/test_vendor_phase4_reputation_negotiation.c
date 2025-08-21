/* Vendor System Phase 4.1–4.5 Tests */
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include "core/vendor/vendor_registry.h"
#include "core/vendor/vendor_reputation.h"
#include <stdio.h>

static int load_items_if_needed(void)
{
    if (rogue_item_defs_count() > 0)
        return 1;
    char p[256];
    if (rogue_find_asset_path("items/swords.cfg", p, sizeof p))
    {
        for (int i = (int) strlen(p) - 1; i >= 0; i--)
        {
            if (p[i] == '/' || p[i] == '\\')
            {
                p[i] = '\0';
                break;
            }
        }
        if (rogue_item_defs_load_directory(p) > 0)
            return 1;
    }
    char pj[256];
    if (!rogue_find_asset_path("items/items.json", pj, sizeof pj))
    {
        rogue_find_asset_path("items.json", pj, sizeof pj);
    }
    if (pj[0])
    {
        if (rogue_item_defs_load_from_json(pj) > 0)
            return 1;
    }
    return 0;
}

int main(void)
{
    if (!load_items_if_needed())
    {
        printf("VENDOR_P4_FAIL items load\n");
        return 1;
    }
    if (!rogue_vendor_registry_load_all())
    {
        printf("VENDOR_P4_FAIL registry load\n");
        return 2;
    }
    rogue_vendor_rep_system_reset();
    if (rogue_vendor_def_count() <= 0)
    {
        printf("VENDOR_P4_FAIL no vendor\n");
        return 3;
    }
    int vidx = 0;
    int last_delta = 1000000;
    for (int i = 0; i < 10; i++)
    {
        int d = rogue_vendor_rep_gain(vidx, 10);
        if (d > last_delta + 1)
        {
            printf("VENDOR_P4_FAIL rep delta increase i=%d prev=%d cur=%d\n", i, last_delta, d);
            return 4;
        }
        last_delta = d;
    }
    const RogueNegotiationRule* rule = rogue_negotiation_rule_find("standard_skill_check");
    if (!rule)
    {
        if (rogue_negotiation_rule_count() <= 0)
        {
            printf("VENDOR_P4_FAIL no negotiation rules\n");
            return 5;
        }
        rule = rogue_negotiation_rule_at(0);
    }
    int success_low = 0;
    int attempts = 20;
    unsigned int now = 0;
    for (int i = 0; i < attempts; i++)
    {
        int succ = 0, locked = 0;
        (void) rogue_vendor_attempt_negotiation(vidx, rule->id, 1, 1, 1, 1, now, 12345u, &succ,
                                                &locked);
        if (!locked)
        {
            now += 11000;
            if (succ)
                success_low++;
        }
        else
        {
            now += 11000;
            i--;
        }
    }
    rogue_vendor_rep_system_reset();
    if (!rogue_vendor_registry_load_all())
    {
        printf("VENDOR_P4_FAIL reload registry\n");
        return 6;
    }
    int success_high = 0;
    now = 0;
    for (int i = 0; i < attempts; i++)
    {
        int succ = 0, locked = 0;
        (void) rogue_vendor_attempt_negotiation(vidx, rule->id, 40, 40, 40, 40, now, 12345u, &succ,
                                                &locked);
        if (!locked)
        {
            now += 11000;
            if (succ)
                success_high++;
        }
        else
        {
            now += 11000;
            i--;
        }
    }
    if (success_high < success_low)
    {
        printf("VENDOR_P4_FAIL negotiation success rate low=%d high=%d\n", success_low,
               success_high);
        return 7;
    }
    printf("VENDOR_PHASE4_REP_NEGOTIATION_OK rep_last_delta=%d success_low=%d success_high=%d\n",
           last_delta, success_low, success_high);
    return 0;
}
