#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    rogue_item_defs_reset();
    int items_added = rogue_item_defs_load_from_cfg("../assets/test_items.cfg");
    assert(items_added >= 3);
    rogue_loot_tables_reset();
    int tables_added = rogue_loot_tables_load_from_cfg("../assets/test_loot_tables.cfg");
    assert(tables_added >= 1);
    int idx = rogue_loot_table_index("GOBLIN_BASIC");
    assert(idx >= 0);
    unsigned int seed = 12345u;
    int out_idx[16]; int out_qty[16];
    int drops = rogue_loot_roll(idx, &seed, 16, out_idx, out_qty);
    assert(drops >= 1);
    for(int i=0;i<drops;i++){
        assert(out_idx[i] >= 0 && out_idx[i] < rogue_item_defs_count());
        assert(out_qty[i] >= 0);
    }
    printf("LOOT_TABLE_ROLL_OK drops=%d\n", drops);
    return 0;
}
