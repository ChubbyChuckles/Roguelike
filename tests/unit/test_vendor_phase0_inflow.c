/* Vendor System Phase 0 (0.4) inflow simulation baseline test */
#include "core/econ_inflow_sim.h"
#include "core/econ_materials.h"
#include "core/loot_item_defs.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <math.h>

static int load_items_all(void){
    char pitems[256]; if(!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems)){ fprintf(stderr,"INFLOW_FAIL find test_items.cfg\n"); return 0; }
    rogue_item_defs_reset(); if(rogue_item_defs_load_from_cfg(pitems) <= 0){ fprintf(stderr,"INFLOW_FAIL load test_items.cfg\n"); return 0; }
    char pmats[256]; if(rogue_find_asset_path("items/materials.cfg", pmats, sizeof pmats)){
        if(rogue_item_defs_load_from_cfg(pmats) <= 0){ fprintf(stderr,"INFLOW_WARN load mats\n"); }
    }
    rogue_econ_material_catalog_build();
    return 1;
}

static int approx(double a, double b){ double diff = fabs(a-b); double tol = 1e-6 * (fabs(a)+fabs(b)+1.0); return diff <= tol; }

int main(void){
    if(!load_items_all()) return 10;
    RogueEconInflowResult r; if(!rogue_econ_inflow_baseline(30, 2.0, 0.6, 0.4, &r)){ fprintf(stderr,"INFLOW_FAIL sim\n"); return 11; }
    if(r.hours != 2.0 || r.kills_per_min != 30){ fprintf(stderr,"INFLOW_FAIL params reflect\n"); return 12; }
    double exp_kills = 2.0 * 60.0 * 30.0;
    if(!approx(r.expected_items, exp_kills * 0.6)){ fprintf(stderr,"INFLOW_FAIL items exp=%f got=%f\n", exp_kills*0.6, r.expected_items); return 13; }
    if(!approx(r.expected_materials, exp_kills * 0.4)){ fprintf(stderr,"INFLOW_FAIL mats exp=%f got=%f\n", exp_kills*0.4, r.expected_materials); return 14; }
    if(r.expected_total_value <= 0.0){ fprintf(stderr,"INFLOW_FAIL total value <=0\n"); return 15; }
    if(!(r.expected_total_value >= r.expected_item_value && r.expected_total_value >= r.expected_material_value)){ fprintf(stderr,"INFLOW_FAIL total composition\n"); return 16; }
    printf("VENDOR_PHASE0_INFLOW_OK total_value=%.2f items=%.2f materials=%.2f\n", r.expected_total_value, r.expected_items, r.expected_materials);
    return 0;
}
