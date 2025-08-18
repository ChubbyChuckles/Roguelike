/* Phase 7 dungeon generation tests: graph, carving, loops, secrets, locks */
#include "world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void init_cfg(RogueWorldGenConfig* cfg){ memset(cfg,0,sizeof *cfg); cfg->seed=777; cfg->width=220; cfg->height=200; cfg->noise_octaves=4; cfg->water_level=0.32; cfg->river_sources=4; cfg->river_max_length=180; }

int main(void){
    RogueWorldGenConfig cfg; init_cfg(&cfg);
    RogueWorldGenContext ctx; rogue_worldgen_context_init(&ctx,&cfg);
    /* Prepare base map just large enough */
    RogueTileMap map; if(!rogue_tilemap_init(&map,cfg.width,cfg.height)){ fprintf(stderr,"alloc fail\n"); return 1; }
    memset(map.tiles, ROGUE_TILE_EMPTY, (size_t)cfg.width*cfg.height);
    RogueDungeonGraph graph; memset(&graph,0,sizeof graph);
    if(!rogue_dungeon_generate_graph(&ctx, 28, 25, &graph)){ fprintf(stderr,"graph gen fail\n"); return 1; }
    if(graph.room_count <= 5){ fprintf(stderr,"expected >5 rooms got %d\n", graph.room_count); return 1; }
    int reachable = rogue_dungeon_validate_reachability(&graph);
    if(reachable != graph.room_count){ fprintf(stderr,"reachability mismatch %d/%d\n", reachable, graph.room_count); return 1; }
    double loop_ratio = rogue_dungeon_loop_ratio(&graph);
    if(loop_ratio < 0.05){ fprintf(stderr,"loop ratio too low %f\n", loop_ratio); return 1; }
    int carved = rogue_dungeon_carve_into_map(&ctx,&map,&graph,0,0,cfg.width,cfg.height);
    if(carved <= 0){ fprintf(stderr,"carving produced no floors\n"); return 1; }
    int locks = rogue_dungeon_place_keys_and_locks(&ctx,&map,&graph);
    int traps = rogue_dungeon_place_traps_and_secrets(&ctx,&map,&graph, 10, 0.15);
    int secrets = rogue_dungeon_secret_room_count(&graph);
    if(locks < 0 || traps < 0){ fprintf(stderr,"negative locks or traps\n"); return 1; }
    if(secrets < 0){ fprintf(stderr,"negative secrets\n"); return 1; }
    /* Tagging validation */
    int treasure=0, elite=0, puzzle=0; for(int i=0;i<graph.room_count;i++){ if(graph.rooms[i].tag & ROGUE_DUNGEON_ROOM_TREASURE) treasure++; if(graph.rooms[i].tag & ROGUE_DUNGEON_ROOM_ELITE) elite++; if(graph.rooms[i].tag & ROGUE_DUNGEON_ROOM_PUZZLE) puzzle++; }
    if(treasure!=1){ fprintf(stderr,"expected exactly 1 treasure room got %d\n", treasure); return 1; }
    if(elite<1){ fprintf(stderr,"expected >=1 elite rooms got %d\n", elite); return 1; }
    if(puzzle<0){ fprintf(stderr,"puzzle negative impossible\n"); return 1; }
    /* Determinism: regenerate graph with same seed & compare room centers */
    RogueDungeonGraph graph2; rogue_worldgen_context_init(&ctx,&cfg); if(!rogue_dungeon_generate_graph(&ctx,28,25,&graph2)){ fprintf(stderr,"graph2 fail\n"); return 1; }
    if(graph2.room_count != graph.room_count){ fprintf(stderr,"room count mismatch %d vs %d\n", graph2.room_count, graph.room_count); return 1; }
    for(int i=0;i<graph.room_count;i++){ int ax=graph.rooms[i].x+graph.rooms[i].w/2; int ay=graph.rooms[i].y+graph.rooms[i].h/2; int bx=graph2.rooms[i].x+graph2.rooms[i].w/2; int by=graph2.rooms[i].y+graph2.rooms[i].h/2; if(ax!=bx||ay!=by){ fprintf(stderr,"room center mismatch %d\n", i); return 1; } }
    rogue_dungeon_free_graph(&graph2);
    rogue_dungeon_free_graph(&graph);
    rogue_tilemap_free(&map);
    rogue_worldgen_context_shutdown(&ctx);
    printf("phase7 dungeon tests passed\n");
    return 0;
}
