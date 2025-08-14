#include "core/loot_item_defs.h"
#include "core/loot_tables.h"
#include "core/loot_adaptive.h"
#include "core/loot_generation.h"
#include "util/log.h"
#include <assert.h>
#include <stdio.h>

/* Simple deterministic stub to test adaptive weighting: we simulate drops for only two categories
   by invoking record directly and recomputing factors. */

static void simulate(int catA, int countA, int catB, int countB){
    for(int i=0;i<countA;i++){
        rogue_adaptive_record_item(catA);
    }
    for(int i=0;i<countB;i++){
        rogue_adaptive_record_item(catB);
    }
    rogue_adaptive_recompute();
}

int main(void){
    rogue_adaptive_reset();
    /* Choose two arbitrary categories within range (0 and 1). */
    int cat0 = 0, cat1 = 1;

    /* Balanced -> factors should remain ~1 (after smoothing). */
    simulate(cat0, 10, cat1, 10);
    float f0 = rogue_adaptive_get_category_factor(cat0);
    float f1 = rogue_adaptive_get_category_factor(cat1);
    assert(f0 > 0.8f && f0 < 1.2f);
    assert(f1 > 0.8f && f1 < 1.2f);

    /* Now skew heavily: add 40 more of cat0 only -> cat1 underrepresented so its factor should rise above 1, cat0 below 1 */
    simulate(cat0, 40, cat1, 0);
    float f0b = rogue_adaptive_get_category_factor(cat0);
    float f1b = rogue_adaptive_get_category_factor(cat1);
    ROGUE_LOG_INFO("Adaptive factors after skew: cat0=%f cat1=%f", f0b, f1b);
    assert(f0b < f0);
    assert(f1b > f1);

    /* Rebalance by adding many cat1 -> factors should move back toward 1. */
    simulate(cat0, 0, cat1, 40);
    float f0c = rogue_adaptive_get_category_factor(cat0);
    float f1c = rogue_adaptive_get_category_factor(cat1);
    assert(f0c > f0b);
    assert(f1c < f1b);

    printf("ADAPTIVE_WEIGHTING_OK f0=%.3f f1=%.3f f0b=%.3f f1b=%.3f\n", f0, f1, f0b, f1b);
    return 0;
}
