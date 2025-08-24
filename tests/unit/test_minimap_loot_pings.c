#include "../../src/core/app_state.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/minimap_loot_pings.h"
#include "../../src/core/path_utils.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("PING_FAIL items_path\n");
        return 2;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("PING_FAIL items_load\n");
        return 3;
    }
    rogue_items_init_runtime();
    rogue_minimap_pings_reset();
    int before = rogue_minimap_pings_active_count();
    int inst = rogue_items_spawn(rogue_item_def_index("epic_axe"), 1, 10.0f, 12.0f);
    if (inst < 0)
    {
        printf("PING_FAIL spawn\n");
        return 4;
    }
    /* spawn should have created a ping */
    int after = rogue_minimap_pings_active_count();
    if (after != before + 1)
    {
        printf("PING_FAIL ping_count before=%d after=%d\n", before, after);
        return 5;
    }
    /* Advance time to expire */
    for (int i = 0; i < 6; i++)
        rogue_minimap_pings_update(1000.0f); /* 6s */
    int expired = rogue_minimap_pings_active_count();
    if (expired != 0)
    {
        printf("PING_FAIL expire remain=%d\n", expired);
        return 6;
    }
    printf("PING_OK spawned=%d before=%d after=%d\n", inst, before, after);
    return 0;
}
