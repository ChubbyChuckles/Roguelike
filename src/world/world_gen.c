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

static double fbm(double x,double y,int octaves,double lacunarity,double gain){
    double amp=1.0, freq=1.0, sum=0.0, norm=0.0;
    for(int i=0;i<octaves;i++){
        sum += value_noise(x*freq,y*freq)*amp;
        norm += amp;
        freq *= lacunarity; amp *= gain;
    }
    return sum / (norm>0?norm:1.0);
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
    if(cfg->advanced_terrain){
        /* Fractal elevation + moisture approach */
    double water_level = (cfg->water_level>0.0? cfg->water_level : 0.32);
    int oct = cfg->noise_octaves>0?cfg->noise_octaves:5;
    double lac = cfg->noise_lacunarity>0.0?cfg->noise_lacunarity:2.0;
    double gain = cfg->noise_gain>0.0?cfg->noise_gain:0.5;
    size_t total = (size_t)map->width * (size_t)map->height;
        static double* elev_field_cache = NULL;
        static double* moist_field_cache = NULL;
        static int cache_w=0, cache_h=0;
        if(cache_w!=map->width || cache_h!=map->height){
            free(elev_field_cache); free(moist_field_cache);
            elev_field_cache = (double*)malloc(total*sizeof(double));
            moist_field_cache = (double*)malloc(total*sizeof(double));
            cache_w=map->width; cache_h=map->height;
        }
        double* elev_field = elev_field_cache;
        double* moist_field = moist_field_cache;
        for(int y=0;y<map->height;y++){
            for(int x=0;x<map->width;x++){
                double nx = (double)x/(double)map->width  - 0.5;
                double ny = (double)y/(double)map->height - 0.5;
                double dist = sqrt(nx*nx+ny*ny);
        double elev = fbm((nx+5.0)*2.0,(ny+7.0)*2.0,oct,lac,gain);
                /* Add continental falloff */
                elev -= dist*0.35;
                /* Moisture separate fbm */
        double moist = fbm((nx+13.0)*2.5,(ny+3.0)*2.5,4,2.0,0.55);
        elev_field[y*map->width + x] = elev;
        moist_field[y*map->width + x] = moist;
                RogueTileType t;
                if(elev < water_level){
                    t = (rng_norm()<0.05)?ROGUE_TILE_RIVER:ROGUE_TILE_WATER; /* some variation */
                } else {
                    double e = elev - water_level;
                    if(e < 0.04){ t = (moist>0.60)?ROGUE_TILE_SWAMP:ROGUE_TILE_GRASS; }
                    else if(e < 0.16){ t = (moist>0.55)?ROGUE_TILE_FOREST:ROGUE_TILE_GRASS; }
                    else if(e < 0.30){ t = (moist>0.70)?ROGUE_TILE_FOREST:ROGUE_TILE_MOUNTAIN; }
                    else if(e < 0.48){ t = (moist<0.35)?ROGUE_TILE_SNOW:ROGUE_TILE_MOUNTAIN; }
                    else { t = (moist<0.45)?ROGUE_TILE_SNOW:ROGUE_TILE_MOUNTAIN; }
                }
                map->tiles[y*map->width + x] = (unsigned char)t;
            }
        }
    /* Attach elevation & moisture arrays temporarily in global statics for later passes */
    /* For simplicity, stash pointers in static locals */
    static double* s_elev=NULL; static double* s_moist=NULL; static int s_w=0,s_h=0; /* limited to single invocation */
    s_elev = elev_field; s_moist = moist_field; s_w = map->width; s_h = map->height;
    /* We'll free them in advanced post stage */
    return; /* skip legacy biome system core path (post handled later) */
    }
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

/* Collapse very small isolated islands ("salt & pepper" noise) to the dominant surrounding terrain. */
static void smooth_small_islands(RogueTileMap* map, int max_island_size, int target_tile /* -1 for any */, int replacement_bias /* not used yet */){
    (void)replacement_bias;
    if(max_island_size <= 0) return;
    int w = map->width, h = map->height;
    size_t total = (size_t)w * (size_t)h;
    unsigned char* visited = (unsigned char*)malloc(total);
    if(!visited) return;
    memset(visited, 0, total);
    int stack_cap = max_island_size * 8; /* heuristic */
    int* sx = (int*)malloc(sizeof(int) * (size_t)stack_cap);
    int* sy = (int*)malloc(sizeof(int) * (size_t)stack_cap);
    if(!sx || !sy){ free(visited); if(sx) free(sx); if(sy) free(sy); return; }
    int* compx = (int*)malloc(sizeof(int) * (size_t)max_island_size * 4);
    int* compy = (int*)malloc(sizeof(int) * (size_t)max_island_size * 4);
    if(!compx || !compy){ free(visited); free(sx); free(sy); if(compx) free(compx); if(compy) free(compy); return; }

    for(int y=0;y<h;y++){
        for(int x=0;x<w;x++){
            size_t idx = (size_t)y * (size_t)w + (size_t)x;
            if(visited[idx]) continue;
            unsigned char tile = map->tiles[idx];
            if(target_tile >= 0 && tile != (unsigned char)target_tile){ visited[idx]=1; continue; }
            if(tile == ROGUE_TILE_RIVER) { visited[idx]=1; continue; } /* keep rivers thin */
            /* Flood fill but abort if component grows beyond limit */
            int comp_count = 0;
            int sp = 0; /* stack pointer */
            sx[sp] = x; sy[sp] = y; sp++;
            visited[idx] = 1;
            int aborted = 0;
            while(sp>0){
                int cx = sx[--sp]; int cy = sy[sp];
                if(comp_count < max_island_size * 4){
                    compx[comp_count] = cx; compy[comp_count] = cy; /* store */
                }
                comp_count++;
                if(comp_count > max_island_size){ aborted = 1; break; }
                static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                for(int di=0; di<4; ++di){
                    int nx = cx + dirs[di][0]; int ny = cy + dirs[di][1];
                    if(nx<0||ny<0||nx>=w||ny>=h) continue;
                    size_t nidx = (size_t)ny * (size_t)w + (size_t)nx;
                    if(visited[nidx]) continue;
                    if(map->tiles[nidx] == tile){
                        if(sp < stack_cap){ sx[sp]=nx; sy[sp]=ny; sp++; }
                        visited[nidx] = 1;
                    }
                }
            }
            /* If component large, skip revisiting remainder (already marked). */
            if(aborted) continue;
            /* If component size <= threshold and not touching edge (optional) */
            if(comp_count <= max_island_size){
                int counts[ROGUE_TILE_MAX]; memset(counts, 0, sizeof counts);
                for(int i=0;i<comp_count;i++){
                    int cx = compx[i]; int cy = compy[i];
                    static const int ndirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                    for(int di=0; di<4; ++di){
                        int nx = cx + ndirs[di][0]; int ny = cy + ndirs[di][1];
                        if(nx<0||ny<0||nx>=w||ny>=h) continue;
                        unsigned char nt = map->tiles[(size_t)ny * (size_t)w + (size_t)nx];
                        if(nt != tile && nt < ROGUE_TILE_MAX) counts[nt]++;
                    }
                }
                int best_type = -1; int best_count = 0;
                for(int t=0;t<ROGUE_TILE_MAX;t++) if(t!=tile && counts[t] > best_count){ best_count = counts[t]; best_type = t; }
                if(best_type >= 0 && best_count > 0){
                    for(int i=0;i<comp_count;i++){
                        int cx = compx[i]; int cy = compy[i];
                        map->tiles[(size_t)cy * (size_t)w + (size_t)cx] = (unsigned char)best_type;
                    }
                }
            }
        }
    }
    free(visited); free(sx); free(sy); free(compx); free(compy);
}

/* Expand water slightly to enforce minimum shore thickness eliminating 1-tile grass notches encroaching into lakes. */
static void thicken_shores(RogueTileMap* map){
    int w=map->width, h=map->height;
    unsigned char* copy = (unsigned char*)malloc((size_t)w*h);
    if(!copy) return;
    memcpy(copy, map->tiles, (size_t)w*h);
    for(int y=1;y<h-1;y++){
        for(int x=1;x<w-1;x++){
            size_t idx = (size_t)y * (size_t)w + (size_t)x;
            unsigned char t = copy[idx];
            if(t==ROGUE_TILE_GRASS || t==ROGUE_TILE_FOREST){
                int water_adj=0; for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){ if(!ox&&!oy) continue; unsigned char nt = copy[(size_t)(y+oy)*w + (size_t)(x+ox)]; if(nt==ROGUE_TILE_WATER||nt==ROGUE_TILE_RIVER) water_adj++; }
                if(water_adj>=5){ map->tiles[idx] = ROGUE_TILE_WATER; }
            }
        }
    }
    free(copy);
}

bool rogue_world_generate(RogueTileMap* out_map, const RogueWorldGenConfig* cfg){
    if(!out_map || !cfg) return false;
    if(!rogue_tilemap_init(out_map, cfg->width, cfg->height)) return false;
    rng_seed(cfg->seed);
    generate_base(out_map, cfg);
    if(!cfg->advanced_terrain){
        generate_caves(out_map, cfg);
        carve_rivers(out_map, cfg);
        apply_erosion(out_map);
    } else {
        /* Advanced post: access elevation data stored in static variables via getter pattern */
        /* Since we used static locals inside generate_base, re-derive them by re-computing elevation (cheap) for features */
        int w = out_map->width, h = out_map->height;
        double water_level = (cfg->water_level>0.0? cfg->water_level : 0.32);
        int oct = cfg->noise_octaves>0?cfg->noise_octaves:5;
        double lac = cfg->noise_lacunarity>0.0?cfg->noise_lacunarity:2.0;
        double gain = cfg->noise_gain>0.0?cfg->noise_gain:0.5;
        double* elev = (double*)malloc((size_t)w*h*sizeof(double));
        if(elev){
            for(int y=0;y<h;y++) for(int x=0;x<w;x++){
                double nx = (double)x/(double)w - 0.5; double ny=(double)y/(double)h - 0.5; double dist=sqrt(nx*nx+ny*ny);
                double e=fbm((nx+5.0)*2.0,(ny+7.0)*2.0,oct,lac,gain); e -= dist*0.35; elev[y*w+x] = e; }
            /* Downhill river tracing */
            int sources = cfg->river_sources>0?cfg->river_sources:8;
            int max_len = cfg->river_max_length>0?cfg->river_max_length: (h*2);
            for(int s=0;s<sources;s++){
                /* pick high elevation starting point */
                int sx=rng_range(0,w-1), sy=rng_range(0,h-1); int attempts=0;
                while(attempts<200){ double e=elev[sy*w+sx]; if(e>water_level+0.25) break; sx=rng_range(0,w-1); sy=rng_range(0,h-1); attempts++; }
                int x=sx, y=sy; double prev_e = elev[y*w+x];
                for(int step=0; step<max_len; ++step){
                    size_t idx=(size_t)y*w+x;
                    out_map->tiles[idx] = (unsigned char)ROGUE_TILE_RIVER;
                    /* widen occasionally */
                    if(step%25==0){
                        for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){
                            if(!ox&&!oy) continue; int nx=x+ox, ny=y+oy; if(nx<0||ny<0||nx>=w||ny>=h) continue;
                            if(rng_norm()<0.4) out_map->tiles[(size_t)ny*w+nx] = (unsigned char)ROGUE_TILE_RIVER_WIDE;
                        }
                    }
                    /* find lowest neighbor */
                    int bestx=x, besty=y; double beste = prev_e; double drop=0.0;
                    for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){
                        if(!ox&&!oy) continue; int nx=x+ox, ny=y+oy; if(nx<0||ny<0||nx>=w||ny>=h) continue; double ne=elev[ny*w+nx]; if(ne < beste){ beste=ne; bestx=nx; besty=ny; drop=prev_e-ne; } }
                    if(bestx==x && besty==y) break; /* local minimum */
                    x=bestx; y=besty; prev_e=beste;
                    if(prev_e < water_level) break; /* reached water */
                }
            }
            /* Delta formation: expand river mouths where river meets water */
            for(int y=1;y<h-1;y++) for(int x=1;x<w-1;x++){
                size_t idx=(size_t)y*w+x; unsigned char t=out_map->tiles[idx];
                if(t==ROGUE_TILE_RIVER||t==ROGUE_TILE_RIVER_WIDE){
                    int water_neighbors=0; for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){ if(!ox&&!oy) continue; unsigned char nt=out_map->tiles[(size_t)(y+oy)*w + (size_t)(x+ox)]; if(nt==ROGUE_TILE_WATER) water_neighbors++; }
                    if(water_neighbors>=4){ out_map->tiles[idx]=(unsigned char)ROGUE_TILE_RIVER_DELTA; }
                }
            }
            /* Mountain-limited caves: generate cellular automata in high elevation mask only */
            double thresh = (cfg->cave_mountain_elev_thresh>0.0?cfg->cave_mountain_elev_thresh: (water_level+0.28));
            int wmask_count=0;
            unsigned char* cave = (unsigned char*)malloc((size_t)w*h);
            if(cave){
                for(int i=0;i<w*h;i++) cave[i]=0;
                for(int y=0;y<h;y++) for(int x=0;x<w;x++){ double e=elev[y*w+x]; if(e>thresh){ cave[y*w+x] = (rng_norm()<cfg->cave_fill_chance)?1:0; wmask_count++; } }
                int iters = cfg->cave_iterations;
                for(int it=0; it<iters; ++it){
                    for(int y=1;y<h-1;y++) for(int x=1;x<w-1;x++){
                        if(!cave[y*w+x]) continue; int count=0; for(int oy=-1;oy<=1;oy++) for(int ox=-1;ox<=1;ox++){ if(!ox&&!oy) continue; if(cave[(y+oy)*w+(x+ox)]) count++; }
                        if(count<4) cave[y*w+x]=0; }
                }
                for(int y=0;y<h;y++) for(int x=0;x<w;x++) if(cave[y*w+x]){ size_t idx=(size_t)y*w+x; if(out_map->tiles[idx]==ROGUE_TILE_MOUNTAIN) out_map->tiles[idx]=(rng_norm()<0.2)?ROGUE_TILE_CAVE_FLOOR:ROGUE_TILE_CAVE_WALL; }
                free(cave);
            }
            free(elev);
        }
    }
    /* Multi-pass small island smoothing: first targeted categories (water, grass, forest, mountain) then global. */
    int passes = cfg->small_island_passes > 0 ? cfg->small_island_passes : 1;
    int max_sz = cfg->small_island_max_size > 0 ? cfg->small_island_max_size : 3;
    for(int p=0;p<passes;p++){
        smooth_small_islands(out_map, max_sz, ROGUE_TILE_WATER, 0);
        smooth_small_islands(out_map, max_sz, ROGUE_TILE_GRASS, 0);
        smooth_small_islands(out_map, max_sz, ROGUE_TILE_FOREST, 0);
        smooth_small_islands(out_map, max_sz, ROGUE_TILE_MOUNTAIN, 0);
        /* final catch-all (target_tile=-1) */
        smooth_small_islands(out_map, max_sz, -1, 0);
    }
    int shore_passes = cfg->shore_fill_passes > 0 ? cfg->shore_fill_passes : 0;
    for(int s=0;s<shore_passes;s++) thicken_shores(out_map);
    return true;
}
