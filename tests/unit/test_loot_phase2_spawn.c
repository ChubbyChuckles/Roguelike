#include "../../src/core/app/app_state.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/util/path_utils.h"
#include <assert.h>
#include <stdio.h>

/* Minimal stub state for loot spawn logic unit test (without full enemy system). */
RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "PATH_FAIL items\n");
        return 10;
    }
    int added = rogue_item_defs_load_from_cfg(pitems);
    if (added < 3)
    {
        fprintf(stderr, "ITEM_LOAD_FAIL count=%d\n", added);
        return 11;
    }
    rogue_loot_tables_reset();
    char ptables[256];
    char sentinel[8] = "SENTIN";
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        fprintf(stderr, "PATH_FAIL tables\n");
        return 12;
    }
    fprintf(stderr, "DBG spawn tables path(before load)='%s' len=%zu sentinel='%s'\n", ptables,
            strlen(ptables), sentinel);
    int tadded = rogue_loot_tables_load_from_cfg(ptables);
    fprintf(stderr, "DBG spawn tables path(after load)='%s' len=%zu sentinel='%s'\n", ptables,
            strlen(ptables), sentinel);
    if (tadded < 1)
    {
        fprintf(stderr, "LOAD_TABLES_FAIL added=%d path=%s\n", tadded, ptables);
        return 1;
    }
    rogue_items_init_runtime();
    unsigned int seed = 123u;
    int idx = rogue_loot_table_index("ORC_BASE");
    assert(idx >= 0);
    int idef[8];
    int qty[8];
    int drops = rogue_loot_roll(idx, &seed, 8, idef, qty);
    if (drops < 1)
    {
        fprintf(stderr, "LOOT_SPAWN_FAIL drops=%d idx=%d entries=%d tables=%d path=%s\n", drops,
                idx, rogue_item_defs_count(), rogue_loot_tables_count(), ptables);
        return 3;
    }
    int before = rogue_items_active_count();
    for (int i = 0; i < drops; i++)
    {
        if (idef[i] >= 0)
            rogue_items_spawn(idef[i], qty[i], 5.0f, 6.0f);
    }
    int after = rogue_items_active_count();
    if (!(after >= before + drops - 1))
    {
        fprintf(stderr, "LOOT_SPAWN_COUNT_FAIL before=%d after=%d drops=%d\n", before, after,
                drops);
        return 4;
    }
    printf("LOOT_SPAWN_OK count=%d\n", after);
    return 0;
}
