#include <assert.h>
#include <string.h>
#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_tables.h"
#include "core/loot/loot_affixes.h"
#include "core/path_utils.h"

/* Phase 23.4: Example config pack load & smoke validation */
int main(void){
    rogue_item_defs_reset();
    rogue_loot_tables_reset();
    rogue_affixes_reset();

    char path[256];
    assert(rogue_find_asset_path("example_pack/items.cfg", path, sizeof path));
    int added_items = rogue_item_defs_load_from_cfg(path);
    assert(added_items==3);

    assert(rogue_find_asset_path("example_pack/loot_tables.cfg", path, sizeof path));
    int added_tables = rogue_loot_tables_load_from_cfg(path);
    assert(added_tables>=1);

    assert(rogue_find_asset_path("example_pack/affixes.cfg", path, sizeof path));
    int added_affixes = rogue_affixes_load_from_cfg(path);
    assert(added_affixes==2);

    /* Basic roll smoke test */
    unsigned int seed=1234u;
    int items[8]; int qty[8]; int rar[8];
    int table_index = rogue_loot_table_index("BASIC");
    assert(table_index>=0);
    int n = rogue_loot_roll_ex(table_index, &seed, 8, items, qty, rar);
    assert(n>=1 && n<=1); /* each table line is single quantity */
    return 0;
}
