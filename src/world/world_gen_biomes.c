/* Base terrain & biome field generation */
#include "world/world_gen_internal.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct { int x,y; RogueTileType base; } BiomeSeed;

static RogueTileType pick_biome(void){
    double r = rng_norm();
    if(r < 0.25) return ROGUE_TILE_GRASS;
    if(r < 0.45) return ROGUE_TILE_FOREST;
    if(r < 0.65) return ROGUE_TILE_MOUNTAIN;
    if(r < 0.80) return ROGUE_TILE_WATER;
    return ROGUE_TILE_GRASS;
}

static RogueTileType elevation_to_tile(double elev){
    if(elev < 0.30) return ROGUE_TILE_WATER;
    if(elev < 0.35) return ROGUE_TILE_GRASS;
    if(elev < 0.55) return ROGUE_TILE_FOREST;
    if(elev < 0.75) return ROGUE_TILE_MOUNTAIN;
    return ROGUE_TILE_MOUNTAIN;
}

void wg_generate_base(RogueTileMap* map, const RogueWorldGenConfig* cfg){
    if(cfg->advanced_terrain){
        double water_level = (cfg->water_level>0.0? cfg->water_level : 0.32);
        int oct = cfg->noise_octaves>0?cfg->noise_octaves:5;
        double lac = cfg->noise_lacunarity>0.0?cfg->noise_lacunarity:2.0;
        double gain = cfg->noise_gain>0.0?cfg->noise_gain:0.5;
        size_t total = (size_t)map->width * (size_t)map->height;
        static double* elev_field_cache = NULL; static double* moist_field_cache = NULL; static int cache_w=0, cache_h=0;
        if(cache_w!=map->width || cache_h!=map->height){ free(elev_field_cache); free(moist_field_cache); elev_field_cache = (double*)malloc(total*sizeof(double)); moist_field_cache = (double*)malloc(total*sizeof(double)); cache_w=map->width; cache_h=map->height; }
        double* elev_field = elev_field_cache; double* moist_field = moist_field_cache;
        for(int y=0;y<map->height;y++) for(int x=0;x<map->width;x++){
            double nx = (double)x/(double)map->width  - 0.5; double ny = (double)y/(double)map->height - 0.5; double dist = sqrt(nx*nx+ny*ny);
            double elev = fbm((nx+5.0)*2.0,(ny+7.0)*2.0,oct,lac,gain); elev -= dist*0.35; double moist = fbm((nx+13.0)*2.5,(ny+3.0)*2.5,4,2.0,0.55); elev_field[y*map->width + x] = elev; moist_field[y*map->width + x] = moist;
            RogueTileType t; if(elev < water_level){ t = (rng_norm()<0.05)?ROGUE_TILE_RIVER:ROGUE_TILE_WATER; } else { double e = elev - water_level; if(e < 0.04){ t = (moist>0.60)?ROGUE_TILE_SWAMP:ROGUE_TILE_GRASS; } else if(e < 0.16){ t = (moist>0.55)?ROGUE_TILE_FOREST:ROGUE_TILE_GRASS; } else if(e < 0.30){ t = (moist>0.70)?ROGUE_TILE_FOREST:ROGUE_TILE_MOUNTAIN; } else if(e < 0.48){ t = (moist<0.35)?ROGUE_TILE_SNOW:ROGUE_TILE_MOUNTAIN; } else { t = (moist<0.45)?ROGUE_TILE_SNOW:ROGUE_TILE_MOUNTAIN; } }
            map->tiles[y*map->width + x] = (unsigned char)t; }
        return; /* skip legacy path */
    }
    int nseeds = cfg->biome_regions; if(nseeds < 1) nseeds = 1; if(nseeds>128) nseeds=128;
    BiomeSeed* seeds = (BiomeSeed*)malloc((size_t)nseeds * sizeof(BiomeSeed));
    for(int i=0;i<nseeds;i++){ seeds[i].x=rng_range(0,map->width-1); seeds[i].y=rng_range(0,map->height-1); seeds[i].base = pick_biome(); }
    for(int y=0;y<map->height;y++) for(int x=0;x<map->width;x++){
        double nx = (double)x / (double)map->width * 8.0; double ny = (double)y / (double)map->height * 8.0; double elev = value_noise(nx,ny);
        int best=0; double bestd=1e9; for(int i=0;i<nseeds;i++){ double dx = (double)(x - seeds[i].x); double dy=(double)(y - seeds[i].y); double d=dx*dx+dy*dy; if(d<bestd){bestd=d;best=i;} }
        RogueTileType t = elevation_to_tile(elev); if(rng_norm()<0.6) t = seeds[best].base; map->tiles[y*map->width + x] = (unsigned char)t; }
    free(seeds);
}
