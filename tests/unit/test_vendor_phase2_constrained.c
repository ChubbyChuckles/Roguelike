/* Vendor System Phase 2.3–2.5 Tests
   - Constrained template-driven generation (uniqueness, rarity caps, guaranteed consumable,
   material & recipe injection)
   - Deterministic reproduction across identical seeds
*/
#include "../../src/core/crafting/crafting.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/vendor/vendor.h"
#include "../../src/core/vendor/vendor_inventory_templates.h"
#include "../../src/core/vendor/vendor_registry.h"
#include <stdio.h>
#include <string.h>

static int count_category(int cat)
{
    int c = 0;
    for (int i = 0; i < rogue_vendor_item_count(); i++)
    {
        const RogueVendorItem* v = rogue_vendor_get(i);
        if (!v)
            continue;
        const RogueItemDef* d = rogue_item_def_at(v->def_index);
        if (!d)
            continue;
        if (d->category == cat)
            c++;
    }
    return c;
}
static int count_rarity(int r)
{
    int c = 0;
    for (int i = 0; i < rogue_vendor_item_count(); i++)
    {
        const RogueVendorItem* v = rogue_vendor_get(i);
        if (v && v->rarity == r)
            c++;
    }
    return c;
}
static int has_duplicate(void)
{
    for (int i = 0; i < rogue_vendor_item_count(); i++)
    {
        const RogueVendorItem* a = rogue_vendor_get(i);
        for (int j = i + 1; j < rogue_vendor_item_count(); j++)
        {
            const RogueVendorItem* b = rogue_vendor_get(j);
            if (a && b && a->def_index == b->def_index)
                return 1;
        }
    }
    return 0;
}

int main(void)
{
    if (!rogue_vendor_registry_load_all())
    {
        printf("VENDOR_P23_FAIL registry load\n");
        return 1;
    }
    if (!rogue_vendor_inventory_templates_load())
    {
        printf("VENDOR_P23_FAIL templates load\n");
        return 2;
    }
    /* Optional crafting recipes (ignore failure) */
    rogue_craft_load_file("assets/crafting/recipes.cfg");
    /* Ensure base item definitions loaded (resolve path robustly) */
    if (rogue_item_defs_count() == 0)
    {
        char pitems[256];
        if (rogue_find_asset_path("items/swords.cfg", pitems, sizeof pitems))
        {
            /* strip filename to directory */
            for (int k = (int) strlen(pitems) - 1; k >= 0; k--)
            {
                if (pitems[k] == '/' || pitems[k] == '\\')
                {
                    pitems[k] = '\0';
                    break;
                }
            }
            rogue_item_defs_load_directory(pitems);
        }
    }
    if (rogue_item_defs_count() == 0)
    {
        printf("VENDOR_P23_FAIL no item defs loaded\n");
        return 11;
    }
    int produced = rogue_vendor_generate_constrained("blacksmith_standard", 123456u, 42, 16);
    if (produced <= 0)
    {
        printf("VENDOR_P23_FAIL produced=%d\n", produced);
        return 3;
    }
    int produced2 = rogue_vendor_generate_constrained("blacksmith_standard", 123456u, 42, 16);
    if (produced2 != produced)
    {
        printf("VENDOR_P23_FAIL nondet count %d %d\n", produced, produced2);
        return 4;
    }
    if (has_duplicate())
    {
        printf("VENDOR_P23_FAIL duplicate items\n");
        return 5;
    }
    if (count_rarity(4) > 1)
    {
        printf("VENDOR_P23_FAIL legendary cap exceeded\n");
        return 6;
    }
    if (count_rarity(3) > 2)
    {
        printf("VENDOR_P23_FAIL epic cap exceeded\n");
        return 7;
    }
    if (count_rarity(2) > 4)
    {
        printf("VENDOR_P23_FAIL rare cap exceeded\n");
        return 8;
    }
    if (count_category(ROGUE_ITEM_CONSUMABLE) < 1)
    {
        printf("VENDOR_P23_FAIL missing consumable\n");
        return 9;
    }
    if (count_category(ROGUE_ITEM_MATERIAL) < 1)
    {
        printf("VENDOR_P23_FAIL missing material\n");
        return 10;
    }
    printf("VENDOR_PHASE2_CONSTRAINED_OK items=%d legendary=%d epic=%d rare=%d consumable=%d "
           "material=%d\n",
           produced, count_rarity(4), count_rarity(3), count_rarity(2),
           count_category(ROGUE_ITEM_CONSUMABLE), count_category(ROGUE_ITEM_MATERIAL));
    return 0;
}
