/* Phase 8 fauna & spawn ecology tests */
#include "world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg){ memset(cfg,0,sizeof *cfg); cfg->seed=1234; cfg->width=64; cfg->height=48; cfg->noise_octaves=3; cfg->water_level=0.30; }

int main(void){
    RogueWorldGenConfig cfg; init_cfg(&cfg);
    RogueWorldGenContext ctx; rogue_worldgen_context_init(&ctx,&cfg);
    RogueTileMap map; if(!rogue_tilemap_init(&map,cfg.width,cfg.height)){ fprintf(stderr,"alloc fail\n"); return 1; }
    /* Simple macro generation to populate grass/forest/water tiles */
    if(!rogue_world_generate_macro_layout(&cfg,&ctx,&map,NULL,NULL)) { fprintf(stderr,"macro gen fail\n"); return 1; }
    /* Register spawn tables */
    rogue_spawn_clear_tables();
    RogueSpawnTable grass={ ROGUE_TILE_GRASS, 250, { {"rat",70,20},{"wolf",25,15},{"boar",5,5} }, 3};
    RogueSpawnTable forest={ ROGUE_TILE_FOREST, 400, { {"wolf",60,25},{"bear",25,15},{"spirit",15,35} }, 3};
    if(rogue_spawn_register_table(&grass)<0 || rogue_spawn_register_table(&forest)<0){ fprintf(stderr,"register fail\n"); return 1; }
    RogueSpawnDensityMap dm; if(!rogue_spawn_build_density(&map,&dm)){ fprintf(stderr,"density fail\n"); return 1; }
    /* Hub suppression center */
    rogue_spawn_apply_hub_suppression(&dm, cfg.width/2, cfg.height/2, 6);
    /* Sample 200 attempts across map */
    int samples=0, rares=0; char id[32]; for(int i=0;i<200;i++){ int x = (i*17)%cfg.width; int y=(i*31)%cfg.height; int is_rare=0; if(rogue_spawn_sample(&ctx,&dm,&map,x,y,id,sizeof id,&is_rare)){ samples++; rares += is_rare; } }
    if(samples==0){ fprintf(stderr,"expected at least some samples\n"); return 1; }
    if(rares < 0){ fprintf(stderr,"rare negative\n"); return 1; }
    /* Determinism: reinit context + resample sequence -> identical first 20 spawn ids */
    char first_ids[20][32]; RogueWorldGenContext ctx2; rogue_worldgen_context_init(&ctx2,&cfg);
    for(int i=0;i<20;i++){ int x=(i*17)%cfg.width; int y=(i*31)%cfg.height; int is_rare=0; if(!rogue_spawn_sample(&ctx2,&dm,&map,x,y,first_ids[i],32,&is_rare)){ strcpy(first_ids[i],"none"); } }
    RogueWorldGenContext ctx3; rogue_worldgen_context_init(&ctx3,&cfg);
    for(int i=0;i<20;i++){ char again[32]; int is_rare=0; int x=(i*17)%cfg.width; int y=(i*31)%cfg.height; if(!rogue_spawn_sample(&ctx3,&dm,&map,x,y,again,32,&is_rare)) strcpy(again,"none"); if(strcmp(again,first_ids[i])!=0){ fprintf(stderr,"nondeterministic spawn %d %s vs %s\n", i, again, first_ids[i]); return 1; } }
    rogue_spawn_free_density(&dm);
    rogue_tilemap_free(&map);
    rogue_worldgen_context_shutdown(&ctx);
    rogue_worldgen_context_shutdown(&ctx2);
    rogue_worldgen_context_shutdown(&ctx3);
    printf("phase8 spawn tests passed\n");
    return 0;
}
