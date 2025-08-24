#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/util/path_utils.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "PATH_FAIL items\n");
        return 10;
    }
    int items_added = rogue_item_defs_load_from_cfg(pitems);
    if (items_added < 3)
    {
        fprintf(stderr, "ITEM_LOAD_FAIL count=%d\n", items_added);
        return 11;
    }
    rogue_loot_tables_reset();
    char ptables[256];
    char sentinel[8] = "SENTIN"; /* detect stack overwrite */
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        fprintf(stderr, "PATH_FAIL tables\n");
        return 12;
    }
    fprintf(stderr, "DBG tables path(before load)='%s' len=%zu sentinel='%s'\n", ptables,
            strlen(ptables), sentinel);
    int tables_added = rogue_loot_tables_load_from_cfg(ptables);
    fprintf(stderr, "DBG tables path(after load)='%s' len=%zu sentinel='%s'\n", ptables,
            strlen(ptables), sentinel);
    if (tables_added < 1)
    {
        fprintf(stderr, "LOAD_TABLES_FAIL added=%d path=%s\n", tables_added, ptables);
        return 1;
    }
    /* Test originally referenced GOBLIN_BASIC which no longer exists; use ORC_BASE from
     * test_loot_tables.cfg */
    int idx = rogue_loot_table_index("ORC_BASE");
    assert(idx >= 0);
    unsigned int seed = 12345u;
    int out_idx[16];
    int out_qty[16];
    int drops = rogue_loot_roll(idx, &seed, 16, out_idx, out_qty);
    /* Table rolls_min=1 so deterministic path should give >=1; treat zero as hard fail */
    if (drops < 1)
    {
        fprintf(
            stderr,
            "LOOT_TABLE_ROLL_FAIL drops=%d idx=%d entries=%d tables=%d tables_added=%d path=%s\n",
            drops, idx, rogue_item_defs_count(), rogue_loot_tables_count(), tables_added, ptables);
        return 2;
    }
    for (int i = 0; i < drops; i++)
    {
        assert(out_idx[i] >= 0 && out_idx[i] < rogue_item_defs_count());
        assert(out_qty[i] >= 0);
    }
    printf("LOOT_TABLE_ROLL_OK drops=%d\n", drops);
    return 0;
}
