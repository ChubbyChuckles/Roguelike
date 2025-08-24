/* Test 7.7 persistence of affixed ground items (round-trip) */
#include "../../src/core/app/app_state.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/persistence/persistence.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>
#include <string.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static int fail(const char* m)
{
    fprintf(stderr, "FAIL:%s\n", m);
    return 1;
}

int main(void)
{
    char apath[256];
    if (!rogue_find_asset_path("affixes.cfg", apath, sizeof apath))
        return fail("affix_path");
    rogue_affixes_reset();
    if (rogue_affixes_load_from_cfg(apath) < 4)
        return fail("affix_load");
    rogue_item_defs_reset();
    if (rogue_item_defs_load_from_cfg("../../assets/test_items.cfg") < 3)
        return fail("item_defs");
    rogue_items_init_runtime();
    /* Prepare one ground item with forced affixes */
    int sword = rogue_item_def_index("long_sword");
    if (sword < 0)
        return fail("sword");
    int inst = rogue_items_spawn(sword, 1, 0, 0);
    if (inst < 0)
        return fail("spawn");
    unsigned int seed = 99u;
    if (rogue_item_instance_generate_affixes(inst, &seed, 3) != 0)
        return fail("gen");
    const RogueItemInstance* it = rogue_item_instance_at(inst);
    if (!it)
        return fail("inst_ptr");
    const char* save_path = "affix_persist_test.cfg";
    rogue_persistence_set_paths("player_stats_ignore.cfg", NULL); /* redirect player stats path */
    /* Save using player stats persistence (re-using same file path g_player_stats_path) */
    rogue_persistence_save_player_stats();
    /* Now append manual save of ground items: included already in save */
    /* Reset runtime and reload */
    RogueItemInstance before = *it;
    rogue_items_shutdown_runtime();
    rogue_items_init_runtime();
    rogue_affixes_reset();
    rogue_affixes_load_from_cfg(apath); /* reload affix defs */
    rogue_item_defs_reset();
    rogue_item_defs_load_from_cfg("../../assets/test_items.cfg");
    rogue_persistence_load_player_stats();
    /* Find at least one active item with matching properties */
    int found = 0;
    for (int i = 0; i < g_app.item_instance_cap; i++)
        if (g_app.item_instances[i].active)
        {
            const RogueItemInstance* ri = &g_app.item_instances[i];
            if (ri->def_index == before.def_index && ri->prefix_index == before.prefix_index &&
                ri->prefix_value == before.prefix_value &&
                ri->suffix_index == before.suffix_index && ri->suffix_value == before.suffix_value)
            {
                found = 1;
                break;
            }
        }
    if (!found)
        return fail("roundtrip");
    printf("AFFIX_PERSIST_ROUNDTRIP_OK prefix=%d pv=%d suffix=%d sv=%d\n", before.prefix_index,
           before.prefix_value, before.suffix_index, before.suffix_value);
    return 0;
}
