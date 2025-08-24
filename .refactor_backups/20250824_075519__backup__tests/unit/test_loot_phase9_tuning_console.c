/* Test 9.6 tuning console commands (set weight, reset counters) */
#include "../../src/core/loot/loot_commands.h"
#include "../../src/core/loot/loot_dynamic_weights.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_stats.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/path_utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void run(const char* cmd)
{
    char buf[128];
    int r = rogue_loot_run_command(cmd, buf, sizeof buf);
    if (r != 0)
    {
        fprintf(stderr, "Command failed: %s => %s\n", cmd, buf);
        assert(0);
    }
}

int main(void)
{
    rogue_loot_dyn_reset();
    rogue_loot_stats_reset();
    /* Load minimal data so rarity rolls can occur */
    char pitems[256];
    char ptables[256];
    int ok_items = rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems);
    assert(ok_items);
    int ok_tables = rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables);
    assert(ok_tables);
    rogue_item_defs_reset();
    int items = rogue_item_defs_load_from_cfg(pitems);
    assert(items > 0);
    rogue_loot_tables_reset();
    int tables = rogue_loot_tables_load_from_cfg(ptables);
    assert(tables > 0);
    int t = rogue_loot_table_index("SKELETON_WARRIOR");
    assert(t >= 0);

    char out[128];
    /* Set a factor via command */
    int rc = rogue_loot_run_command("weight 4 25", out, sizeof out);
    assert(rc == 0);
    assert(strstr(out, "r4") != NULL);
    assert(rogue_loot_dyn_get_factor(4) > 1.0f);
    /* Query it */
    rc = rogue_loot_run_command("get 4", out, sizeof out);
    assert(rc == 0);
    assert(strstr(out, "FACTOR") != NULL);

    /* Produce some rolls to generate stats */
    unsigned int seed = 123;
    int idef[16], qty[16], rar[16];
    for (int i = 0; i < 50; i++)
    {
        rogue_loot_roll_ex(t, &seed, 16, idef, qty, rar);
    }
    rc = rogue_loot_run_command("stats", out, sizeof out);
    assert(rc == 0);
    assert(strstr(out, "STATS:") != NULL);

    /* Reset stats and ensure zeros */
    run("reset_stats");
    rc = rogue_loot_run_command("stats", out, sizeof out);
    assert(rc == 0);
    /* Expect all zeros after reset */
    assert(strstr(out, "C=0") != NULL);

    /* Reset dyn and ensure factor returns to ~1 */
    run("reset_dyn");
    assert(rogue_loot_dyn_get_factor(4) >= 0.99f && rogue_loot_dyn_get_factor(4) <= 1.01f);

    printf("LOOT_TUNING_CONSOLE_OK\n");
    return 0;
}
