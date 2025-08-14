/* Test 5.4 dynamic rarity weighting adjustments */
#include "core/loot_tables.h"
#include "core/loot_item_defs.h"
#include "core/loot_dynamic_weights.h"
#include <assert.h>
#include <stdio.h>

int main(void){
    rogue_loot_dyn_reset();
    rogue_item_defs_reset(); int items = rogue_item_defs_load_from_cfg("../../assets/test_items.cfg"); assert(items>0);
    rogue_loot_tables_reset(); int tables = rogue_loot_tables_load_from_cfg("../../assets/test_loot_tables.cfg"); assert(tables>0);
    int t = rogue_loot_table_index("SKELETON_WARRIOR"); assert(t>=0);
    unsigned int seedA=123u, seedB=123u;
    /* Baseline sample rarities */
    int idef[32], qty[32], rar_base[32]; int drops1 = rogue_loot_roll_ex(t,&seedA,32,idef,qty,rar_base); assert(drops1>0);
    /* Heavy bias legendary */
    rogue_loot_dyn_set_factor(4, 50.0f);
    unsigned int seedC=123u; int rar_bias[32]; int drops2 = rogue_loot_roll_ex(t,&seedC,32,idef,qty,rar_bias); assert(drops2==drops1);
    int count_leg_base=0,count_leg_bias=0; for(int i=0;i<drops1;i++){ if(rar_base[i]==4) count_leg_base++; if(rar_bias[i]==4) count_leg_bias++; }
    if(!(count_leg_bias >= count_leg_base)){
        fprintf(stderr,"FAIL: bias not applied base=%d bias=%d\n", count_leg_base, count_leg_bias); return 1;
    }
    printf("DYNAMIC_RARITY_WEIGHTS_OK base_leg=%d bias_leg=%d\n", count_leg_base, count_leg_bias);
    return 0;
}
