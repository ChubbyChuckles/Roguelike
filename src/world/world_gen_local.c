/* Phase 4: Local Terrain & Caves Implementation
 * Provides chunk-local detail noise perturbation, cave refinement layer separate from macro pass,
 * lava/water pocket placement, ore vein carving, and passability map derivation.
 * Deterministic via provided RogueWorldGenContext RNG channels (micro channel for fine detail).
 */
#include "world/world_gen.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward decl to existing fbm noise */
double fbm(double x,double y,int octaves,double lacunarity,double gain);

static double prand_norm(RogueRngChannel* ch){ return rogue_worldgen_rand_norm(ch); }
static int prand_range(RogueRngChannel* ch,int lo,int hi){ if(hi<=lo) return lo; double r=prand_norm(ch); int span = hi-lo+1; int v = lo + (int)(r*span); if(v>hi) v=hi; return v; }

bool rogue_world_generate_local_terrain(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map){
    if(!cfg||!ctx||!io_map||!io_map->tiles) return false; int w=io_map->width, h=io_map->height; int oct = cfg->noise_octaves>0?cfg->noise_octaves:4; double lac = cfg->noise_lacunarity>0?cfg->noise_lacunarity:2.0; double gain = cfg->noise_gain>0?cfg->noise_gain:0.5;
    /* Apply subtle perturbation: convert some grass to forest / mountain edges using micro noise */
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){
        int idx=y*w+x; unsigned char t = io_map->tiles[idx]; if(t==ROGUE_TILE_GRASS || t==ROGUE_TILE_FOREST){ double n = fbm((x+13)*0.15,(y+7)*0.15, oct,lac,gain); if(n>0.55 && t==ROGUE_TILE_GRASS){ io_map->tiles[idx]=ROGUE_TILE_FOREST; } else if(n< -0.15 && t==ROGUE_TILE_FOREST){ io_map->tiles[idx]=ROGUE_TILE_GRASS; } }
        if(io_map->tiles[idx]==ROGUE_TILE_MOUNTAIN){ double n2 = fbm((x+5)*0.21,(y+11)*0.21, oct,lac,gain); if(n2>0.65) io_map->tiles[idx]=ROGUE_TILE_GRASS; }
    }
    return true;
}

bool rogue_world_generate_caves_layer(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map){
    if(!cfg||!ctx||!io_map||!io_map->tiles) return false; int w=io_map->width, h=io_map->height; unsigned char* cur = (unsigned char*)malloc((size_t)w*h); unsigned char* nxt=(unsigned char*)malloc((size_t)w*h); if(!cur||!nxt){ if(cur) free(cur); if(nxt) free(nxt); return false; }
    /* Seed only under mountains using initial fill chance */
    double fill = cfg->cave_fill_chance>0? cfg->cave_fill_chance:0.45; /* adjust slightly upward for tighter caves */
    fill += 0.10; if(fill>0.90) fill=0.90;
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int idx=y*w+x; unsigned char t=io_map->tiles[idx]; if(t==ROGUE_TILE_MOUNTAIN){ cur[idx] = (prand_norm(&ctx->micro_rng) < fill)?1:0; } else cur[idx]=0; }
    int iters = cfg->cave_iterations>0?cfg->cave_iterations:3; for(int it=0; it<iters; ++it){ for(int y=0;y<h;y++) for(int x=0;x<w;x++){ int idx=y*w+x; int count_n=0; for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){ if(!ox&&!oy) continue; int nx=x+ox, ny=y+oy; if(nx<0||ny<0||nx>=w||ny>=h) { count_n++; continue; } if(cur[ny*w+nx]) count_n++; } unsigned char curv=cur[idx]; /* tighten rules slightly to reduce openness */ unsigned char nv = curv ? (count_n>=5?1:0) : (count_n>=6?1:0); nxt[idx]=nv; } unsigned char* tmp=cur; cur=nxt; nxt=tmp; }
    int wall=0,floor=0; for(int i=0;i<w*h;i++){ if(cur[i]){ io_map->tiles[i]=ROGUE_TILE_CAVE_WALL; wall++; } else if(io_map->tiles[i]==ROGUE_TILE_MOUNTAIN){ io_map->tiles[i]=ROGUE_TILE_CAVE_FLOOR; floor++; } }
    /* Deterministic post-adjustment to keep openness within [0.25,0.75] */
    if(wall+floor>0){
        double open = (double)floor/(double)(wall+floor);
        /* If too sparse (rare) we could carve extra floors later; for now handle only excessive openness. */
        while(open > 0.75){
            /* Convert a batch of random floor cells to walls */
            int target_batch = (int)((open-0.74)*(wall+floor)); if(target_batch < 1) target_batch = 1; /* at least one */
            for(int attempt=0; attempt<target_batch; ++attempt){ int idx = prand_range(&ctx->micro_rng,0,w*h-1); if(io_map->tiles[idx]==ROGUE_TILE_CAVE_FLOOR){ io_map->tiles[idx]=ROGUE_TILE_CAVE_WALL; floor--; wall++; } }
            open = (double)floor/(double)(wall+floor);
            if(target_batch > (wall+floor)) break; /* safety */
        }
    }
    free(cur); free(nxt); return true;
}

bool rogue_world_place_lava_and_liquids(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map, int target_pockets){
    if(!cfg||!ctx||!io_map) return false; int w=io_map->width,h=io_map->height; if(target_pockets<=0) return true; int placed=0; int attempts=0; while(placed<target_pockets && attempts<target_pockets*20){ attempts++; int x=prand_range(&ctx->micro_rng,1,w-2); int y=prand_range(&ctx->micro_rng,1,h-2); int idx=y*w+x; if(io_map->tiles[idx]!=ROGUE_TILE_CAVE_FLOOR) continue; /* flood small pocket */ int radius=prand_range(&ctx->micro_rng,1,3); for(int oy=-radius;oy<=radius;oy++) for(int ox=-radius;ox<=radius;ox++){ int nx=x+ox, ny=y+oy; if(nx<0||ny<0||nx>=w||ny>=h) continue; double d=hypot((double)ox,(double)oy); if(d<=radius && io_map->tiles[ny*w+nx]==ROGUE_TILE_CAVE_FLOOR){ io_map->tiles[ny*w+nx]=ROGUE_TILE_LAVA; } } placed++; }
    return true;
}

bool rogue_world_place_ore_veins(const RogueWorldGenConfig* cfg, RogueWorldGenContext* ctx, RogueTileMap* io_map, int target_veins, int vein_len){
    if(!cfg||!ctx||!io_map) return false; int w=io_map->width, h=io_map->height; if(target_veins<=0||vein_len<=0) return true; int created=0; int safety=0; while(created<target_veins && safety<target_veins*50){ safety++; int x=prand_range(&ctx->micro_rng,0,w-1); int y=prand_range(&ctx->micro_rng,0,h-1); if(io_map->tiles[y*w+x]!=ROGUE_TILE_CAVE_WALL) continue; int len=0; int dir = prand_range(&ctx->micro_rng,0,3); int dx[4]={1,-1,0,0}; int dy[4]={0,0,1,-1}; int cx=x, cy=y; while(len<vein_len){ int idx=cy*w+cx; if(io_map->tiles[idx]==ROGUE_TILE_CAVE_WALL){ io_map->tiles[idx]=ROGUE_TILE_ORE_VEIN; } if(prand_norm(&ctx->micro_rng)<0.3) dir = prand_range(&ctx->micro_rng,0,3); cx+=dx[dir]; cy+=dy[dir]; if(cx<0||cy<0||cx>=w||cy>=h) break; len++; }
        created++;
    }
    return true;
}

bool rogue_world_build_passability(const RogueWorldGenConfig* cfg, const RogueTileMap* map, RoguePassabilityMap* out_pass){
    if(!cfg||!map||!out_pass) return false; int w=map->width,h=map->height; size_t count=(size_t)w*h; out_pass->width=w; out_pass->height=h; out_pass->walkable=(unsigned char*)malloc(count); if(!out_pass->walkable) return false; for(size_t i=0;i<count;i++){ unsigned char t=map->tiles[i]; unsigned char walk=0; switch(t){ case ROGUE_TILE_GRASS: case ROGUE_TILE_FOREST: case ROGUE_TILE_SWAMP: case ROGUE_TILE_SNOW: case ROGUE_TILE_CAVE_FLOOR: case ROGUE_TILE_RIVER_DELTA: walk=1; break; default: walk=0; break; } out_pass->walkable[i]=walk; }
    return true;
}
void rogue_world_passability_free(RoguePassabilityMap* pass){ if(!pass) return; free(pass->walkable); pass->walkable=NULL; }
