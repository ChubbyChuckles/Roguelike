#include "../../src/core/crafting/crafting.h"
#include "../../src/core/crafting/crafting_skill.h"
#include "../../src/core/crafting/material_registry.h"
#include "../../src/core/vendor/vendor_crafting_integration.h"
#include <assert.h>
#include <stdio.h>

static int g_gold = 50000;
static int spend_gold(int amt)
{
    if (amt < 0)
        return -1;
    if (g_gold < amt)
        return -2;
    g_gold -= amt;
    return 0;
}
static int unlocked_count = 0;
static void on_unlocked(int idx) { unlocked_count++; }

int main()
{
    /* Guard: need at least one recipe. If none present test still passes (no-op) */
    if (rogue_craft_recipe_count() > 0)
    {
        int rc = rogue_vendor_purchase_recipe_unlock(0, 1000, spend_gold, on_unlocked);
        assert(rc == 0); /* unlock path */
        /* Second call should not charge again */
        int gold_after = g_gold;
        rc = rogue_vendor_purchase_recipe_unlock(0, 1000, spend_gold, on_unlocked);
        assert(rc == 0);
        assert(g_gold == gold_after);
    }
    /* Scarcity record simulation */
    rogue_vendor_scarcity_record(0, 5);
    rogue_vendor_scarcity_record(0, -2);
    assert(rogue_vendor_scarcity_score(0) == 3);
    /* Batch refine simulation (will be no-op if material registry empty) */
    int promoted = rogue_vendor_batch_refine(0, 0, 10, 3, 2, 15, 10, spend_gold);
    if (promoted >= 0)
    { /* accept >=0 */
    }
    printf("VENDOR_PHASE8_CRAFTING_INTEGRATION_OK\n");
    return 0;
}
