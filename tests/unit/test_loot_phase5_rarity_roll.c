#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    rogue_item_defs_reset();
    int items = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg");
    assert(items>=1);
    rogue_loot_tables_reset();
    int tables = rogue_loot_tables_load_from_cfg("../../assets/test_loot_tables.cfg");
    assert(tables>=1);
    int idx = rogue_loot_table_index("ORC_BASE"); assert(idx>=0);
    unsigned int seed=123u; int idef[16]; int qty[16]; int rar[16];
    int drops = rogue_loot_roll_ex(idx,&seed,16,idef,qty,rar); assert(drops>=1);
    for(int i=0;i<drops;i++){ assert(idef[i]>=0); assert(qty[i]>=0); if(rar[i]!=-1){ assert(rar[i] >=0 && rar[i] <=4); } }
    printf("LOOT_RARITY_ROLL_OK drops=%d\n", drops);
    return 0;
}
