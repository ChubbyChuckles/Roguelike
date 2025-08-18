#define SDL_MAIN_HANDLED 1
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "core/economy.h"

int main(void){
    /* Monotonic: higher missing durability => higher or equal cost */
    int rarity=2; int lvl=1; int last=0; for(int miss=1; miss<=200; ++miss){ int c = rogue_econ_repair_cost_ex(miss, rarity, lvl); assert(c>=last); last=c; }
    /* Rarity scaling: higher rarity cost/unit > lower given same missing + level */
    int miss=50; int c0 = rogue_econ_repair_cost_ex(miss,0,10); int c4 = rogue_econ_repair_cost_ex(miss,4,10); assert(c4>c0);
    /* Level scaling: cost increases with level */
    int l1 = rogue_econ_repair_cost_ex(miss,2,1); int l50 = rogue_econ_repair_cost_ex(miss,2,50); int l200 = rogue_econ_repair_cost_ex(miss,2,200); assert(l50>l1 && l200>l50);
    printf("equipment_phase8_repair_cost_ok\n");
    return 0;
}
