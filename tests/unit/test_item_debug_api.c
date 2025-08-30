#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/core/loot/item_debug.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/util/asset_config.h"

static int try_load_items(void)
{
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
        return -1;
    int added = rogue_item_defs_load_from_cfg(pitems);
    return added;
}

int main(void)
{
    rogue_item_defs_reset();
    int added = try_load_items();
    assert(added > 0);

    int count = rogue_item_debug_count();
    assert(count > 0);

    const RogueItemDef* d0 = rogue_item_debug_get(0);
    assert(d0);

    // Edit a few fields and verify roundtrip via getters
    int rc = rogue_item_debug_set_int(0, "level_req", d0->level_req + 1);
    assert(rc == 0);
    rc = rogue_item_debug_set_int(0, "stack_max", d0->stack_max + 2);
    assert(rc == 0);
    rc = rogue_item_debug_set_int(0, "base_damage_min", d0->base_damage_min + 3);
    assert(rc == 0);
    rc = rogue_item_debug_set_int(0, "base_damage_max", d0->base_damage_max + 4);
    assert(rc == 0);

    char new_name[64];
    snprintf(new_name, sizeof new_name, "%s_EDIT", d0->name);
    rc = rogue_item_debug_set_name(0, new_name);
    assert(rc == 0);

    const RogueItemDef* d1 = rogue_item_debug_get(0);
    assert(d1);
    assert(strcmp(d1->name, new_name) == 0);
    assert(d1->base_damage_max >= d0->base_damage_max);

    // Save to JSON and reload; loader returns number added (>=0)
    // Write into the current working directory (ctest runs from build/),
    // so avoid a nested 'build/' folder that may not exist.
    const char* path = "test_items_overrides.json";
    rc = rogue_item_debug_save_json(path);
    assert(rc == 0);

    int loaded = rogue_item_debug_load_json(path);
    assert(loaded >= 0);

    // Bad field should fail
    assert(rogue_item_debug_set_int(0, "does_not_exist", 123) < 0);

    printf("OK test_item_debug_api with %d items (added=%d)\n", count, added);
    return 0;
}
