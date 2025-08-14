#include "core/economy.h"
#include <stdio.h>
#include <stdlib.h>

/* Simple unit test for repair & reroll currency sink helpers */
int main(void){
    rogue_econ_reset();
    int cost_r0 = rogue_econ_repair_cost(10,0); /* expect 10 * 5 = 50 */
    int cost_r4 = rogue_econ_repair_cost(10,4); /* expect 10 * (5+4*5)=10*25=250 */
    if(cost_r0 != 50 || cost_r4 != 250){ fprintf(stderr,"REPAIR_COST_FAIL %d %d\n",cost_r0,cost_r4); return 1; }

    int reroll0 = rogue_econ_reroll_affix_cost(0); /* 50 */
    int reroll3 = rogue_econ_reroll_affix_cost(3); /* 50 * 8 = 400 */
    if(reroll0 != 50 || reroll3 != 400){ fprintf(stderr,"REROLL_COST_FAIL %d %d\n",reroll0,reroll3); return 2; }

    printf("SINK_OK %d %d %d %d\n", cost_r0,cost_r4,reroll0,reroll3);
    return 0;
}
