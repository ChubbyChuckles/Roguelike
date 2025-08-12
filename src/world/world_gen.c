#include "world/world_gen.h"
#include <stdlib.h>
#include <math.h>

/* Simple xorshift RNG for deterministic generation */
static unsigned int rng_state;
static void rng_seed(unsigned int s){ if(!s) s = 1; rng_state = s; }
static unsigned int rng_u32(void){ unsigned int x=rng_state; x^=x<<13; x^=x>>17; x^=x<<5; return rng_state=x; }
static double rng_norm(void){ return (double)rng_u32() / (double)0xffffffffu; }
static int rng_range(int lo,int hi){ return lo + (int)(rng_norm()*(double)(hi-lo+1)); }

/* Value noise (2D) */
static double hash2(int x,int y){ unsigned int h = (unsigned int)(x*374761393 + y*668265263); h = (h^(h>>13))*1274126177u; return (double)(h & 0xffffff)/ (double)0xffffff; }
static double lerp(double a,double b,double t){ return a + (b-a)*t; }
static double smoothstep(double t){ return t*t*(3.0-2.0*t); }
static double value_noise(double x,double y){
    int xi=(int)floor(x), yi=(int)floor(y); double tx=x-xi, ty=y-yi;
    double v00=hash2(xi,yi), v10=hash2(xi+1,yi), v01=hash2(xi,yi+1), v11=hash2(xi+1,yi+1);
    double sx=smoothstep(tx), sy=smoothstep(ty);
    double a=lerp(v00,v10,sx); double b=lerp(v01,v11,sx); return lerp(a,b,sy);
}

/* Voronoi biome seeds */
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

static void generate_base(RogueTileMap* map, const RogueWorldGenConfig* cfg){
    int nseeds = cfg->biome_regions; if(nseeds < 1) nseeds = 1; if(nseeds>128) nseeds=128;
    BiomeSeed* seeds = (BiomeSeed*)malloc((size_t)nseeds * sizeof(BiomeSeed));
    for(int i=0;i<nseeds;i++){ seeds[i].x=rng_range(0,map->width-1); seeds[i].y=rng_range(0,map->height-1); seeds[i].base = pick_biome(); }
    for(int y=0;y<map->height;y++){
        for(int x=0;x<map->width;x++){
            /* Elevation via noise */
            double nx = (double)x / (double)map->width * 8.0;
            double ny = (double)y / (double)map->height * 8.0;
            double elev = value_noise(nx,ny);
            /* Voronoi - find nearest seed */
            int best=0; double bestd=1e9;
            for(int i=0;i<nseeds;i++){ double dx = (double)(x - seeds[i].x); double dy=(double)(y - seeds[i].y); double d=dx*dx+dy*dy; if(d<bestd){bestd=d;best=i;} }
            RogueTileType t = elevation_to_tile(elev);
            /* Blend with biome seed base (simple override chance) */
            if(rng_norm()<0.6) t = seeds[best].base;
            map->tiles[y*map->width + x] = (unsigned char)t;
        }
    }
    free(seeds);
}

static void generate_caves(RogueTileMap* map, const RogueWorldGenConfig* cfg){
    int w=map->width, h=map->height;
    unsigned char* cave = (unsigned char*)malloc((size_t)w*h);
    for(int i=0;i<w*h;i++) cave[i] = (rng_norm() < cfg->cave_fill_chance) ? 1 : 0;
    int iters = cfg->cave_iterations;
    for(int it=0; it<iters; ++it){
        for(int y=0;y<h;y++){
            for(int x=0;x<w;x++){
                int count=0; for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){ if(ox==0&&oy==0) continue; int nx=x+ox, ny=y+oy; if(nx<0||ny<0||nx>=w||ny>=h){ count++; } else if(cave[ny*w+nx]) count++; }
                int idx=y*w+x; unsigned char cur=cave[idx];
                if(cur){ cave[idx] = (count>=4) ? 1 : 0; } else { cave[idx] = (count>=5) ? 1 : 0; }
            }
        }
    }
    for(int y=0;y<h;y++) for(int x=0;x<w;x++){ if(cave[y*w+x]) map->tiles[y*w+x] = (unsigned char)ROGUE_TILE_CAVE_WALL; else if(map->tiles[y*w+x]==ROGUE_TILE_MOUNTAIN) map->tiles[y*w+x] = (unsigned char)ROGUE_TILE_CAVE_FLOOR; }
    free(cave);
}

static void carve_river(RogueTileMap* map){
    int w=map->width, h=map->height;
    int x = rng_range(0,w-1); int y=0;
    for(int steps=0; steps<h*2 && y<h; ++steps){
        map->tiles[y*w+x] = (unsigned char)ROGUE_TILE_RIVER;
        /* random walk biased downward */
        int dir = rng_range(0,9); /* 0-9 */
        if(dir < 5) y++; /* mostly go down */
        else if(dir < 7) x++; else if(dir < 9) x--; else y++;
        if(x<0) x=0; if(x>=w) x=w-1;
    }
}

static void carve_rivers(RogueTileMap* map, const RogueWorldGenConfig* cfg){
    int attempts = cfg->river_attempts; if(attempts<0) attempts=0; if(attempts>16) attempts=16;
    for(int i=0;i<attempts;i++) carve_river(map);
}

static void apply_erosion(RogueTileMap* map){
    int w=map->width, h=map->height;
    for(int y=1;y<h-1;y++){
        for(int x=1;x<w-1;x++){
            unsigned char* t=&map->tiles[y*w+x];
            if(*t==ROGUE_TILE_MOUNTAIN){
                int water=0; for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){ if(!ox&&!oy) continue; unsigned char n=map->tiles[(y+oy)*w+(x+ox)]; if(n==ROGUE_TILE_WATER||n==ROGUE_TILE_RIVER) water++; }
                if(water>=3 && rng_norm()<0.4) *t=ROGUE_TILE_GRASS;
            }
        }
    }
}

bool rogue_world_generate(RogueTileMap* out_map, const RogueWorldGenConfig* cfg){
    if(!out_map || !cfg) return false;
    if(!rogue_tilemap_init(out_map, cfg->width, cfg->height)) return false;
    rng_seed(cfg->seed);
    generate_base(out_map, cfg);
    generate_caves(out_map, cfg);
    carve_rivers(out_map, cfg);
    apply_erosion(out_map);
    return true;
}
