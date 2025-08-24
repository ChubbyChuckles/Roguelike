/* Phase 20.5 Stress Test: Max ground items + merge behavior
 * - Spawns many identical stackable items clustered to trigger merges.
 * - Verifies active count well below spawn attempts (merges occurred) and never exceeds cap.
 * - Ensures no crash and merge ratio reasonable (>20% merges).
 */
#include "../../src/core/app_state.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
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
        fprintf(stderr, "FAIL: find test_items.cfg (wd issue)\n");
        return 1;
    }
    int load_items = rogue_item_defs_load_from_cfg(pitems);
    if (load_items <= 0)
    {
        fprintf(stderr, "FAIL: load test_items.cfg rc=%d path=%s\n", load_items, pitems);
        return 2;
    }

    rogue_loot_tables_reset();
    char ptables[256];
    if (!rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables))
    {
        fprintf(stderr, "FAIL: find test_loot_tables.cfg (wd issue)\n");
        return 3;
    }
    int load_tables = rogue_loot_tables_load_from_cfg(ptables);
    if (load_tables <= 0)
    {
        fprintf(stderr, "FAIL: load test_loot_tables.cfg rc=%d path=%s\n", load_tables, ptables);
        return 4;
    }

    int item_count = rogue_item_defs_count();
    fprintf(stderr, "DIAG: item_defs_count=%d load_items=%d\n", item_count, load_items);

    rogue_items_init_runtime();
    int gold_index = rogue_item_def_index("gold_coin");
    if (gold_index < 0)
    {
        fprintf(stderr, "FAIL: gold_coin index <0 (item defs not loaded?) count=%d\n", item_count);
        return 5;
    }

    int attempts = 200;
    int spawned = 0;
    for (int i = 0; i < attempts; i++)
    {
        float ox = 5.0f + (float) (i % 3) * 0.2f;
        float oy = 5.0f + (float) ((i / 3) % 3) *
                              0.2f; /* small grid ensures adjacency within merge radius */
        int inst = rogue_items_spawn(gold_index, 1, ox, oy);
        if (inst >= 0)
            spawned++;
        else
        {
            /* first few diagnostics */
            if (i < 3)
                fprintf(stderr, "DIAG: spawn_fail iter=%d def_index=%d\n", i, gold_index);
        }
    }
    /* Trigger merge pass (recent refactor moved merging to update loop). */
    rogue_items_update(0.0f);
    int active = rogue_items_active_count();
    if (active > ROGUE_ITEM_INSTANCE_CAP)
    {
        fprintf(stderr, "FAIL: active exceeds cap %d>%d\n", active, ROGUE_ITEM_INSTANCE_CAP);
        return 6;
    }
    int merges = spawned - active;
    if (merges <= 0)
    {
        fprintf(stderr, "FAIL: no merges occurred spawned=%d active=%d (gold_index=%d)\n", spawned,
                active, gold_index);
        return 7;
    }
    float merge_ratio = (float) merges / (float) spawned;
    if (merge_ratio < 0.20f)
    {
        fprintf(stderr, "FAIL: insufficient merge ratio %.2f spawned=%d active=%d merges=%d\n",
                merge_ratio, spawned, active, merges);
        return 8;
    }
    printf("loot_merge_stress_ok spawned=%d active=%d merges=%d ratio=%.2f\n", spawned, active,
           merges, merge_ratio);
    return 0;
}
