#include "../../src/core/loot/loot_filter.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>

static int fail(const char* m)
{
    fprintf(stderr, "FAIL:%s\n", m);
    return 1;
}

int main(void)
{
    char pitems[256];
    if (!rogue_find_asset_path("items/swords.cfg", pitems, sizeof pitems))
        return fail("path_items");
    // Trim to directory
    for (int i = (int) strlen(pitems) - 1; i >= 0; i--)
    {
        if (pitems[i] == '/' || pitems[i] == '\\')
        {
            pitems[i] = '\0';
            break;
        }
    }
    rogue_item_defs_reset();
    if (rogue_item_defs_load_directory(pitems) <= 0)
        return fail("load_defs");
    rogue_items_init_runtime();
    int common = rogue_item_def_index("iron_sword");
    int epic = rogue_item_def_index("epic_blade");
    if (common < 0 || epic < 0)
        return fail("defs");
    int inst_common = rogue_items_spawn(common, 1, 0, 0);
    int inst_epic = rogue_items_spawn(epic, 1, 0, 0);
    if (inst_common < 0 || inst_epic < 0)
        return fail("spawn");
    if (rogue_items_visible_count() != 2)
        return fail("vis2");
    // Build filter file dynamically
    const char* fname = "loot_filter_test.cfg";
    FILE* f = NULL;
    f = fopen(fname, "wb");
    if (!f)
        return fail("tmpfile");
    fputs("rarity>=3\n", f);
    fclose(f);
    if (rogue_loot_filter_reset() != 0)
        return fail("reset");
    if (rogue_loot_filter_load(fname) <= 0)
        return fail("load");
    rogue_loot_filter_refresh_instances();
    if (rogue_items_visible_count() != 1)
        return fail("vis1");
    const RogueItemInstance* epic_inst = rogue_item_instance_at(inst_epic);
    const RogueItemInstance* com_inst = rogue_item_instance_at(inst_common);
    if (!epic_inst || !com_inst)
        return fail("inst_null");
    if (epic_inst->hidden_filter)
        return fail("epic_hidden");
    if (!com_inst->hidden_filter)
        return fail("common_not_hidden");
    printf("LOOT_FILTER_OK rules=%d visible=%d hidden=%d\n", rogue_loot_filter_rule_count(),
           rogue_items_visible_count(), rogue_items_active_count() - rogue_items_visible_count());
    return 0;
}
