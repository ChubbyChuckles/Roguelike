#include "core/loot/loot_item_defs.h"
#include "core/vendor/vendor_special_offers.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    rogue_vendor_offers_reset();
    /* Load item definitions so rarity distribution (incl rarity 4) is available */
    char pitems[256];
    if (rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        rogue_item_defs_reset();
        int items = rogue_item_defs_load_from_cfg(pitems);
        assert(items > 0);
    }
    unsigned int now = 0;
    unsigned int seed = 12345u;
    int added = rogue_vendor_offers_roll(seed, now, 0);
    assert(added >= 0);
    int count1 = rogue_vendor_offers_count();
    int added2 = rogue_vendor_offers_roll(seed + 1, now + 1000, 0);
    int count2 = rogue_vendor_offers_count();
    assert(count2 >= count1);
    for (int i = 0; i < 15; i++)
    {
        rogue_vendor_offers_roll(seed + 2 + i, now + 11 * 60 * 1000 + (unsigned int) (i * 10), 0);
    }
    int after_expire = rogue_vendor_offers_count();
    assert(after_expire >= 0 && after_expire <= ROGUE_VENDOR_OFFER_SLOT_CAP);
    rogue_vendor_offers_reset();
    int rare_hits = 0;
    for (int i = 0; i < 80; i++)
    {
        rogue_vendor_offers_roll(seed + 100 + i, now, 1);
        for (int j = 0; j < rogue_vendor_offers_count(); j++)
        {
            const RogueVendorSpecialOffer* o = rogue_vendor_offer_get(j);
            if (o && o->rarity == 4)
                rare_hits++;
        }
    }
    assert(rare_hits > 0);
    printf("VENDOR_PHASE6_SPECIAL_OFFERS_OK\n");
    return 0;
}
