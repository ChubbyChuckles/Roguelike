/* Phase 4 unit tests: local terrain perturbation, caves, lava pockets, ore veins, passability determinism */
#include "world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg){ memset(cfg,0,sizeof *cfg); cfg->seed=1337; cfg->width=96; cfg->height=96; cfg->noise_octaves=4; cfg->noise_gain=0.5; cfg->noise_lacunarity=2.0; cfg->cave_fill_chance=0.45; cfg->cave_iterations=3; }

int main(void){
    RogueWorldGenConfig cfg; init_cfg(&cfg);
    RogueWorldGenContext ctx; rogue_worldgen_context_init(&ctx,&cfg);
    RogueTileMap map; if(!rogue_tilemap_init(&map,cfg.width,cfg.height)){ fprintf(stderr,"alloc fail\n"); return 1; }
    /* Generate macro first to supply base tiles */
    if(!rogue_world_generate_macro_layout(&cfg,&ctx,&map,NULL,NULL)){ fprintf(stderr,"macro gen fail\n"); return 1; }
    /* Re-init context to ensure deterministic layering independent of macro RNG consumption order */
    rogue_worldgen_context_init(&ctx,&cfg);
    if(!rogue_world_generate_local_terrain(&cfg,&ctx,&map)){ fprintf(stderr,"local terrain fail\n"); return 1; }
    if(!rogue_world_generate_caves_layer(&cfg,&ctx,&map)){ fprintf(stderr,"caves fail\n"); return 1; }
    if(!rogue_world_place_lava_and_liquids(&cfg,&ctx,&map,6)){ fprintf(stderr,"lava fail\n"); return 1; }
    if(!rogue_world_place_ore_veins(&cfg,&ctx,&map,10,18)){ fprintf(stderr,"veins fail\n"); return 1; }
    /* Stats */
    int w=map.width,h=map.height; int total=w*h; int cave_floor=0,cave_wall=0,lava=0,ore=0; for(int i=0;i<total;i++){ unsigned char t=map.tiles[i]; if(t==ROGUE_TILE_CAVE_FLOOR) cave_floor++; else if(t==ROGUE_TILE_CAVE_WALL) cave_wall++; else if(t==ROGUE_TILE_LAVA) lava++; else if(t==ROGUE_TILE_ORE_VEIN) ore++; }
    if(cave_floor + cave_wall + ore == 0){ fprintf(stderr,"expected some cave tiles\n"); return 1; }
    /* Openness ratio bounds (floor / (solid walls including ore)) */
    double openness = (double)cave_floor / (double)(cave_floor + cave_wall + ore);
    if(openness < 0.25 || openness > 0.75){ fprintf(stderr,"openness out of bounds %.2f\n", openness); return 1; }
    if(lava == 0){ fprintf(stderr,"expected lava pockets\n"); return 1; }
    if(ore == 0){ fprintf(stderr,"expected ore veins\n"); return 1; }
    /* Passability determinism */
    RoguePassabilityMap pass1, pass2; if(!rogue_world_build_passability(&cfg,&map,&pass1)){ fprintf(stderr,"pass1 fail\n"); return 1; }
    /* Rebuild pass map again to verify deterministic identical result */
    if(!rogue_world_build_passability(&cfg,&map,&pass2)){ fprintf(stderr,"pass2 fail\n"); return 1; }
    if(pass1.width!=pass2.width || pass1.height!=pass2.height || memcmp(pass1.walkable, pass2.walkable, (size_t)pass1.width*pass1.height)!=0){ fprintf(stderr,"passability mismatch\n"); return 1; }
    unsigned long long h1=0,h2=0; for(int i=0;i<total;i++){ h1 = h1*1315423911u + map.tiles[i]; }
    /* Determinism check: regenerate full phase chain and compare hash */
    RogueTileMap map2; rogue_tilemap_init(&map2,cfg.width,cfg.height); rogue_worldgen_context_init(&ctx,&cfg); rogue_world_generate_macro_layout(&cfg,&ctx,&map2,NULL,NULL); rogue_worldgen_context_init(&ctx,&cfg); rogue_world_generate_local_terrain(&cfg,&ctx,&map2); rogue_world_generate_caves_layer(&cfg,&ctx,&map2); rogue_world_place_lava_and_liquids(&cfg,&ctx,&map2,6); rogue_world_place_ore_veins(&cfg,&ctx,&map2,10,18); for(int i=0;i<total;i++){ h2 = h2*1315423911u + map2.tiles[i]; }
    if(h1!=h2){ fprintf(stderr,"determinism mismatch %llu vs %llu\n", (unsigned long long)h1,(unsigned long long)h2); return 1; }
    rogue_world_passability_free(&pass1); rogue_world_passability_free(&pass2); rogue_tilemap_free(&map); rogue_tilemap_free(&map2); rogue_worldgen_context_shutdown(&ctx);
    printf("phase4 local terrain & caves tests passed\n");
    return 0;
}
