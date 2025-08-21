/* Phase 20.2 / 20.3 Statistical harness + regression tolerance for rarity probability
 * Strategy:
 *  - Roll a selected loot table many times (100k) capturing rarity outcomes for a target item across its allowed band.
 *  - Compute empirical frequencies; assert each allowed rarity appears and distribution not overly skewed.
 *    (Current sampler applies dynamic weights external to this test; baseline uniform expected within band.)
 *  - Tolerance: no rarity frequency may deviate from uniform expectation by more than ABS_TOL (0.15 absolute).
 *  - Provides early warning if future weighting logic or pity/floor adjustments inadvertently bias mid-band drastically.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "core/loot/loot_tables.h"
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include "../../src/core/loot/loot_dynamic_weights.h"
#include "../../src/core/loot/loot_drop_rates.h"
#include "core/loot/loot_rarity_adv.h"

static int nearly_eq(float a, float b, float tol){ return fabsf(a-b) <= tol; }

int main(void){
    rogue_loot_dyn_reset();
    rogue_drop_rates_reset();
    rogue_rarity_adv_reset();
    rogue_item_defs_reset(); char pitems[256]; assert(rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)); assert(rogue_item_defs_load_from_cfg(pitems)>0);
    rogue_loot_tables_reset(); char ptables[256]; assert(rogue_find_asset_path("test_loot_tables.cfg", ptables, sizeof ptables)); assert(rogue_loot_tables_load_from_cfg(ptables)>0);

    /* Test rarity band directly (0-2) using sampler to isolate probability shifts */
    int rmin=0, rmax=2; int span = rmax - rmin + 1; float expected = 1.0f / (float)span; const float ABS_TOL = 0.15f;

    int counts[5]={0,0,0,0,0};
    unsigned int rng=0xBEEFu;
    const int iterations = 100000; /* Enough to stabilize distribution */
    for(int it=0; it<iterations; ++it){
        int rar = rogue_loot_rarity_sample(&rng, rmin, rmax);
        if(rar>=rmin && rar<=rmax) counts[rar]++;
    }
    int total = counts[0]+counts[1]+counts[2];
    if(total==0){ fprintf(stderr,"FAIL: no rarities sampled\n"); return 2; }
    for(int r=rmin; r<=rmax; ++r){
        float freq = (float)counts[r] / (float)total;
        if(fabsf(freq - expected) > ABS_TOL){
            fprintf(stderr,"FAIL: rarity %d frequency %.4f outside tolerance (expected ~%.4f +/- %.2f)\n", r, freq, expected, ABS_TOL);
            return 10;
        }
    }
    /* Ensure all appeared */
    for(int r=rmin; r<=rmax; ++r){ if(counts[r]==0){ fprintf(stderr,"FAIL: rarity %d never appeared\n", r); return 11; } }
    printf("loot_stats_regression_ok total=%d f0=%.4f f1=%.4f f2=%.4f\n", total, (float)counts[0]/total, (float)counts[1]/total, (float)counts[2]/total);
    return 0;
}
