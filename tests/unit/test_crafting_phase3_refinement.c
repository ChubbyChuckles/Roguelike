#include "core/crafting/material_refine.h"
#include "core/crafting/material_registry.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <assert.h>

static void assert_true(int cond, const char* msg){ if(!cond){ fprintf(stderr,"FAIL: %s\n", msg); assert(cond); } }

int main(void){
    /* Load item defs for linking (materials registry requires item def indices) */
    if(rogue_item_defs_load_directory("assets/items") <=0 && rogue_item_defs_load_directory("../assets/items") <=0){ fprintf(stderr,"item defs load fail\n"); return 2; }
    if(rogue_material_registry_load_path("assets/materials/materials.cfg") <=0 &&
       rogue_material_registry_load_path("../assets/materials/materials.cfg") <=0){ fprintf(stderr,"materials registry load fail\n"); return 2; }

    rogue_material_quality_reset();
    /* Use first registry material (assume at least one) */
    if(rogue_material_count() <=0){ fprintf(stderr,"no materials in registry\n"); return 3; }
    int mat0 = 0;
    /* Add 100 units at quality 10 */
    assert_true(rogue_material_quality_add(mat0,10,100)==0, "add quality 10");
    assert_true(rogue_material_quality_count(mat0,10)==100, "count q10=100");
    unsigned int rng=12345u; int produced=0; int crit=0;
    int rc = rogue_material_refine(mat0,10,20,40,&rng,&produced,&crit);
    assert_true(rc==0, "refine success");
    assert_true(produced>=0, "produced non-negative");
    int avg = rogue_material_quality_average(mat0); assert_true(avg>=10 && avg<=25, "avg within expected bounds");
    float bias = rogue_material_quality_bias(mat0); assert_true(bias>=0.0f && bias<=1.0f, "bias bounds");
    /* Force multiple refinements to exercise failure & crit statistically */
    for(int i=0;i<10;i++){ rogue_material_quality_add(mat0,10,30); unsigned int before_rng=rng; rc=rogue_material_refine(mat0,10,20,30,&rng,&produced,&crit); assert_true(rc==0 || rc==-3, "refine rc range"); }
    printf("CRAFT_P3_OK mat=%d total=%d avg=%d bias=%.2f\n", mat0, rogue_material_quality_total(mat0), rogue_material_quality_average(mat0), rogue_material_quality_bias(mat0));
    return 0;
}
