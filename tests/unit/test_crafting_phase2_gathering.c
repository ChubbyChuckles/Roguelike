#include "core/crafting/gathering.h"
#include "core/crafting/material_registry.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Minimal stub world seed & chunk iteration test for Phase 2 gathering */

static void assert_true(int cond, const char* msg){ if(!cond){ fprintf(stderr,"FAIL: %s\n", msg); assert(cond); } }

int main(void){
    /* Load item definitions directory first (assets/items). */
    if(rogue_item_defs_load_directory("assets/items") <= 0 && rogue_item_defs_load_directory("../assets/items") <=0){
        fprintf(stderr,"Could not load item defs directory\n"); return 2; }
    /* Load registry from new materials asset directly to avoid relying on default search during tests. */
    if(rogue_material_registry_load_path("assets/materials/materials.cfg") <=0 &&
       rogue_material_registry_load_path("../assets/materials/materials.cfg") <=0 &&
       rogue_material_registry_load_path("../../assets/materials/materials.cfg") <=0){
        fprintf(stderr,"Could not load materials registry asset\n"); return 2; }

    /* Provide an inline synthetic definition set instead of relying on external file for determinism. */
    const char* cfg_path = "gather_test_nodes.cfg";
    FILE* f = fopen(cfg_path, "wb");
    if(!f){ fprintf(stderr,"Cannot create temp cfg file\n"); return 3; }
    /* id, material_table, min_roll, max_roll, respawn_ms, tool_req_tier, biome_tags, spawn%, rare%, rare_mult */
    fprintf(f, "iron_vein,arcane_dust_mat:1;arcane_dust_mat:1,1,3,50,1,overworld,100,50,3.0\n");
    fprintf(f, "copper_vein,arcane_dust_mat:1,2,4,10,0,overworld,100,0,2.0\n");
    fclose(f);

    int loaded = rogue_gather_defs_load_path(cfg_path);
    assert_true(loaded==2, "loaded two node defs");

    unsigned int world_seed = 12345u;
    int spawned_chunk0 = rogue_gather_spawn_chunk(world_seed, 0);
    assert_true(spawned_chunk0>0, "spawned nodes in chunk 0");
    int node_count = rogue_gather_node_count();
    assert_true(node_count==spawned_chunk0, "node count matches spawned");

    /* Tool gating check: first expect failure for tool tier 0 on iron_vein requiring 1 */
    rogue_gather_set_player_tool_tier(0);
    unsigned int rng=world_seed;
    int mat_def= -1, qty=-1;
    int rc = rogue_gather_harvest(0,&rng,&mat_def,&qty);
    assert_true(rc==-3, "harvest blocked by tool tier");

    rogue_gather_set_player_tool_tier(2); /* upgrade tool */
    rc = rogue_gather_harvest(0,&rng,&mat_def,&qty);
    assert_true(rc==0, "harvest success after tool upgrade");
    assert_true(qty>=1 && qty<=3*3, "qty within expected rare-multiplied upper bound");

    /* Node should now be depleted */
    rc = rogue_gather_harvest(0,&rng,&mat_def,&qty);
    assert_true(rc==-2, "cannot harvest depleted node");

    /* Simulate time passage for respawn */
    for(int i=0;i<60;i++){ rogue_gather_update(1.0f); }
    rc = rogue_gather_harvest(0,&rng,&mat_def,&qty);
    assert_true(rc==0, "harvest after respawn");

    /* Determinism: spawn chunk again with same seed should add zero new nodes since we keep global list; but fresh spawn count with same seed increments if we reset - omitted for simplicity */

    printf("CRAFT_P2_OK defs=%d nodes=%d first_qty=%d total_harvests=%llu rare=%llu\n", loaded, node_count, qty, rogue_gather_total_harvests(), rogue_gather_total_rare_procs());
    fflush(stdout);
    return 0;
}
