/* Test vendor restock & rotation (10.3) */
#include "../../src/core/loot/loot_drop_rates.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/path_utils.h"
#include "../../src/core/vendor/vendor.h"
#include <stdio.h>

int main(void)
{
    char pitems[256];
    char ptables[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "RESTOCK_FAIL find items\n");
        return 10;
    }
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        fprintf(stderr, "RESTOCK_FAIL find tables\n");
        return 11;
    }
    rogue_item_defs_reset();
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        fprintf(stderr, "RESTOCK_FAIL load items\n");
        return 12;
    }
    rogue_drop_rates_reset();
    rogue_loot_tables_reset();
    if (rogue_loot_tables_load_from_cfg(ptables) <= 0)
    {
        fprintf(stderr, "RESTOCK_FAIL load loot tables\n");
        return 13;
    }
    int t1 = rogue_loot_table_index("SKELETON_WARRIOR");
    int t2 = rogue_loot_table_index(
        "SKELETON_WARRIOR"); /* using same for simplicity, rotation logic independent */
    if (t1 < 0 || t2 < 0)
    {
        fprintf(stderr, "RESTOCK_FAIL table index\n");
        return 14;
    }
    RogueVendorRotation rot;
    rogue_vendor_rotation_init(&rot, 1000.0f); /* 1s interval */
    rogue_vendor_rotation_add_table(&rot, t1);
    rogue_vendor_rotation_add_table(&rot, t2);
    RogueGenerationContext ctx = {
        .enemy_level = 8, .biome_id = 0, .enemy_archetype = 2, .player_luck = 1};
    unsigned int seed = 777u;
    /* Initial manual restock */
    rogue_vendor_reset();
    rogue_vendor_generate_inventory(rogue_vendor_current_table(&rot), 4, &ctx, &seed);
    int first_count = rogue_vendor_item_count();
    if (first_count <= 0)
    {
        fprintf(stderr, "RESTOCK_FAIL initial gen=%d\n", first_count);
        return 20;
    }
    /* Advance less than interval: expect no restock */
    if (rogue_vendor_update_and_maybe_restock(&rot, 500.0f, &ctx, &seed, 4) != 0)
    {
        fprintf(stderr, "RESTOCK_FAIL unexpected early restock\n");
        return 21;
    }
    int count_mid = rogue_vendor_item_count();
    if (count_mid != first_count)
    {
        fprintf(stderr, "RESTOCK_FAIL count changed early\n");
        return 22;
    }
    /* Advance past interval: expect restock */
    int restocked = rogue_vendor_update_and_maybe_restock(&rot, 600.0f, &ctx, &seed, 4);
    if (!restocked)
    {
        fprintf(stderr, "RESTOCK_FAIL no restock after interval\n");
        return 23;
    }
    int new_count = rogue_vendor_item_count();
    if (new_count <= 0)
    {
        fprintf(stderr, "RESTOCK_FAIL new_count=%d\n", new_count);
        return 24;
    }
    printf("VENDOR_RESTOCK_OK old=%d new=%d\n", first_count, new_count);
    return 0;
}
