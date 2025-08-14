#include "core/vegetation.h"
#include "graphics/font.h"
#include "graphics/tile_sprites.h"
#include "util/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#define ROGUE_MAX_VEG_DEFS 256
#define ROGUE_MAX_VEG_INSTANCES 4096

static RogueVegetationDef g_defs[ROGUE_MAX_VEG_DEFS];
static int g_def_count = 0;
static RogueVegetationInstance g_instances[ROGUE_MAX_VEG_INSTANCES];
static int g_instance_count = 0;
static float g_target_tree_cover = 0.12f; /* fraction of grass tiles covered by tree canopy */
static unsigned int g_last_seed = 0;

static unsigned int vrng_state = 1u;
static void vrng_seed(unsigned int s){ if(!s) s=1; vrng_state = s; }
static unsigned int vrng_u32(void){ unsigned int x=vrng_state; x^=x<<13; x^=x>>17; x^=x<<5; return vrng_state=x; }
static double vrng_norm(void){ return (double)vrng_u32() / (double)0xffffffffu; }

/* Simple value noise for clustering */
static float vhash(int x,int y){ unsigned int h=(unsigned int)(x*374761393u + y*668265263u); h=(h^(h>>13))*1274126177u; return (float)(h & 0xffffff)/16777215.0f; }
static float vlerp(float a,float b,float t){ return a + (b-a)*t; }
static float vsmooth(float t){ return t*t*(3.0f-2.0f*t); }
static float vnoise2(float x,float y){ int xi=(int)floorf(x); int yi=(int)floorf(y); float tx=x-xi; float ty=y-yi; float v00=vhash(xi,yi); float v10=vhash(xi+1,yi); float v01=vhash(xi,yi+1); float v11=vhash(xi+1,yi+1); float sx=vsmooth(tx); float sy=vsmooth(ty); float a=vlerp(v00,v10,sx); float b=vlerp(v01,v11,sx); return vlerp(a,b,sy); }
static float fbm2(float x,float y,int oct){ float sum=0,amp=1,tot=0; for(int i=0;i<oct;i++){ sum += vnoise2(x,y)*amp; tot+=amp; x*=2.0f; y*=2.0f; amp*=0.5f; } return (tot>0)? sum/tot:0.0f; }

void rogue_vegetation_init(void){ g_def_count=0; g_instance_count=0; }
void rogue_vegetation_clear_instances(void){ g_instance_count=0; }
void rogue_vegetation_shutdown(void){ g_def_count=0; g_instance_count=0; }

static int parse_line(char* line, int is_tree){
    /* Expected formats (extended):
       PLANT,id,path,tx1,ty1,tx2,ty2,rarity
       TREE,id,path,tx1,ty1,tx2,ty2,rarity,canopy_radius
       Backwards compatibility:
       PLANT,id,path,tx,ty,rarity
       TREE,id,path,tx,ty,rarity,canopy_radius
    */
    char* context=NULL; char* tok = strtok_s(line, ",\r\n", &context); if(!tok) return 0;
    if((is_tree && strcmp(tok,"TREE")!=0) || (!is_tree && strcmp(tok,"PLANT")!=0)) return 0;
    if(g_def_count>=ROGUE_MAX_VEG_DEFS) return 0;
    RogueVegetationDef* d = &g_defs[g_def_count]; memset(d,0,sizeof *d);
    d->is_tree = (unsigned char)is_tree;
    tok = strtok_s(NULL, ",\r\n", &context); if(!tok) return 0; strncpy_s(d->id, sizeof d->id, tok, _TRUNCATE);
    tok = strtok_s(NULL, ",\r\n", &context); if(!tok) return 0; strncpy_s(d->image, sizeof d->image, tok, _TRUNCATE);
    /* tx1 */
    tok = strtok_s(NULL, ",\r\n", &context); if(!tok) return 0; d->tile_x = (unsigned short)atoi(tok);
    /* ty1 */
    tok = strtok_s(NULL, ",\r\n", &context); if(!tok) return 0; d->tile_y = (unsigned short)atoi(tok);
    /* Attempt to parse extended rectangle form: next tokens may be tx2, ty2 */
    char* save_ptr = context; char* look = strtok_s(NULL, ",\r\n", &context); if(!look) return 0; int have_rect = 0; int v1 = atoi(look);
    char* look2_ctx = context; char* look2 = strtok_s(NULL, ",\r\n", &look2_ctx); if(look2){ /* Could be ty2 */
        int v2 = atoi(look2);
        /* Peek further to decide if these are rx,ry (must still have rarity afterwards) */
        char* look3_ctx = look2_ctx; char* look3 = strtok_s(NULL, ",\r\n", &look3_ctx);
        if(look3){ /* Treat look & look2 as tx2,ty2 and look3 as rarity */
            d->tile_x2 = (unsigned short)v1;
            d->tile_y2 = (unsigned short)v2;
            d->rarity = (unsigned short)atoi(look3);
            context = look3_ctx;
            have_rect = 1;
        }
    }
    if(!have_rect){
        /* Backwards compatibility: the token we read is rarity */
        d->tile_x2 = d->tile_x;
        d->tile_y2 = d->tile_y;
        d->rarity = (unsigned short)v1; /* look held rarity */
        context = save_ptr; /* ensure context points after rarity already consumed */
    }
    if(d->rarity==0) d->rarity=1;
    if(is_tree){ char* cr = strtok_s(NULL, ",\r\n", &context); if(!cr) return 0; d->canopy_radius = (unsigned char)atoi(cr); if(d->canopy_radius==0) d->canopy_radius=1; }
    else d->canopy_radius = 0;
    if(!have_rect){ d->tile_x2 = d->tile_x; d->tile_y2 = d->tile_y; }
    g_def_count++;
    return 1;
}

static int open_with_fallback(const char* base, FILE** out){
    const char* variants[3]={ base, NULL, NULL};
    static char buf1[256]; static char buf2[256];
    snprintf(buf1,sizeof buf1,"../%s", base); variants[1]=buf1;
    snprintf(buf2,sizeof buf2,"../../%s", base); variants[2]=buf2;
    for(int i=0;i<3;i++){
        FILE* f=NULL; fopen_s(&f, variants[i], "r"); if(f){ *out=f; return i; }
    }
    return -1;
}
int rogue_vegetation_load_defs(const char* plants_cfg, const char* trees_cfg){
    FILE* f=NULL; int which=open_with_fallback(plants_cfg,&f); if(f){ char line[512]; while(fgets(line,sizeof line,f)){ if(line[0]=='#' || line[0]=='\n') continue; char tmp[512]; strncpy_s(tmp,sizeof tmp,line,_TRUNCATE); parse_line(tmp,0); } fclose(f);} else { ROGUE_LOG_WARN("plants.cfg not found (tried %s, ../%s, ../../%s)", plants_cfg, plants_cfg, plants_cfg); }
    f=NULL; which=open_with_fallback(trees_cfg,&f); if(f){ char line[512]; while(fgets(line,sizeof line,f)){ if(line[0]=='#' || line[0]=='\n') continue; char tmp[512]; strncpy_s(tmp,sizeof tmp,line,_TRUNCATE); parse_line(tmp,1); } fclose(f);} else { ROGUE_LOG_WARN("trees.cfg not found (tried %s, ../%s, ../../%s)", trees_cfg, trees_cfg, trees_cfg); }
    ROGUE_LOG_INFO("Vegetation defs loaded: %d", g_def_count);
    return g_def_count;
}

static void pick_weighted(int want_tree, int* out_index){
    unsigned int total=0; for(int i=0;i<g_def_count;i++){ if(g_defs[i].is_tree==(want_tree!=0)) total += g_defs[i].rarity; }
    if(total==0){ *out_index=-1; return; }
    unsigned int r = (unsigned int)(vrng_norm()*total); unsigned int acc=0;
    for(int i=0;i<g_def_count;i++){ if(g_defs[i].is_tree==(want_tree!=0)){ acc += g_defs[i].rarity; if(r < acc){ *out_index=i; return; } } }
    *out_index=-1;
}

void rogue_vegetation_generate(float tree_cover_target, unsigned int seed){
    if(tree_cover_target < 0.0f) tree_cover_target = 0.0f; if(tree_cover_target>0.70f) tree_cover_target=0.70f; /* safety */
    g_target_tree_cover = tree_cover_target; g_last_seed = seed; vrng_seed(seed);
    g_instance_count=0;
    /* Collect grass tiles */
    int w=g_app.world_map.width, h=g_app.world_map.height; if(!g_app.world_map.tiles) return;
    int grass_count=0; for(int i=0;i<w*h;i++){ unsigned char t=g_app.world_map.tiles[i]; if(t==ROGUE_TILE_GRASS) grass_count++; }
    if(grass_count==0) return;
    /* Determine desired tree count from cover (approx canopy area ratio) */
    int desired_tree_canopy_tiles = (int)floor(grass_count * g_target_tree_cover + 0.5f);
    int placed_canopy_tiles=0; int attempts=0; const int max_attempts = desired_tree_canopy_tiles * 40 + 2000;
    /* Precompute low-frequency noise field guiding clustering: high values favor trees */
    const float inv_w = 1.0f/(float)w; const float inv_h=1.0f/(float)h;
    while(placed_canopy_tiles < desired_tree_canopy_tiles && attempts < max_attempts){
        attempts++;
        int gx = (int)(vrng_norm() * w); int gy = (int)(vrng_norm() * h);
        if(gx<2||gy<2||gx>=w-2||gy>=h-2) continue; /* margin */
        unsigned char base = g_app.world_map.tiles[gy*w+gx]; if(base!=ROGUE_TILE_GRASS) continue;
        float nx = (float)gx * inv_w; float ny = (float)gy * inv_h;
        float density = fbm2(nx*6.0f + 3.0f, ny*6.0f + 11.0f, 3); /* 0..1 */
        if(density < 0.48f) continue; /* skip to enforce groves */
        /* Local repulsion: ensure no other tree within radius unless density very high */
        int idx; pick_weighted(1,&idx); if(idx<0) break; RogueVegetationDef* d=&g_defs[idx]; int r=d->canopy_radius; int blocked=0;
        for(int oi=0; oi<g_instance_count && !blocked; ++oi){ if(g_instances[oi].is_tree){ float dx=g_instances[oi].x - (gx+0.5f); float dy=g_instances[oi].y - (gy+0.5f); float dist2=dx*dx+dy*dy; float min_r=(float)(g_defs[g_instances[oi].def_index].canopy_radius + r) * 0.85f; if(dist2 < min_r*min_r && density < 0.78f){ blocked=1; break; } } }
        if(blocked) continue;
        for(int oy=-r;oy<=r && !blocked;oy++) for(int ox=-r;ox<=r && !blocked;ox++){ int tx=gx+ox, ty=gy+oy; unsigned char t=g_app.world_map.tiles[ty*w+tx]; if(t!=ROGUE_TILE_GRASS && t!=ROGUE_TILE_FOREST){ blocked=1; break; } }
        if(blocked) continue;
        if(g_instance_count>=ROGUE_MAX_VEG_INSTANCES) break;
        RogueVegetationInstance* inst = &g_instances[g_instance_count++]; inst->x=(float)gx+0.5f; inst->y=(float)gy+0.5f; inst->def_index=(unsigned short)idx; inst->is_tree=1; inst->variant=0; inst->growth=255;
        placed_canopy_tiles += (int)(3.14159f * d->canopy_radius * d->canopy_radius * 0.55f); /* approximate fill factor */
    }
    /* Plants: sprinkle proportionally to leftover grass (density heuristic) */
    int desired_plants = grass_count / 42; if(desired_plants> (ROGUE_MAX_VEG_INSTANCES - g_instance_count - 64)) desired_plants = (ROGUE_MAX_VEG_INSTANCES - g_instance_count - 64);
    for(int p=0;p<desired_plants;p++){
        int tries=0; while(tries<40){ tries++; int gx=(int)(vrng_norm()*w); int gy=(int)(vrng_norm()*h); if(gx<0||gy<0||gx>=w||gy>=h) continue; unsigned char t=g_app.world_map.tiles[gy*w+gx]; if(t!=ROGUE_TILE_GRASS) continue; float nx=(float)gx*inv_w; float ny=(float)gy*inv_h; float noise=fbm2(nx*10.0f+19.0f, ny*10.0f+7.0f, 2); if(noise < 0.35f) continue; int idx; pick_weighted(0,&idx); if(idx<0) break; if(g_instance_count>=ROGUE_MAX_VEG_INSTANCES) break; RogueVegetationInstance* inst=&g_instances[g_instance_count++]; inst->x=(float)gx+0.5f; inst->y=(float)gy+0.5f; inst->def_index=(unsigned short)idx; inst->is_tree=0; inst->variant=0; inst->growth=200; break; }
    }
}

void rogue_vegetation_set_tree_cover(float cover_pct){ rogue_vegetation_generate(cover_pct, g_last_seed? g_last_seed: 12345u); }
float rogue_vegetation_get_tree_cover(void){ return g_target_tree_cover; }

/* --- Sprite rendering support -------------------------------------------------- */
#ifdef ROGUE_HAVE_SDL
typedef struct VegSheetTex { char path[128]; RogueTexture tex; } VegSheetTex;
static VegSheetTex g_sheet_textures[64];
static int g_sheet_tex_count = 0;

static RogueTexture* veg_get_texture(const char* path){
    for(int i=0;i<g_sheet_tex_count;i++) if(strcmp(g_sheet_textures[i].path,path)==0) return &g_sheet_textures[i].tex;
    if(g_sheet_tex_count >= (int)(sizeof g_sheet_textures/sizeof g_sheet_textures[0])) return NULL; /* out of slots */
    VegSheetTex* slot=&g_sheet_textures[g_sheet_tex_count]; memset(slot,0,sizeof *slot); strncpy_s(slot->path,sizeof slot->path,path,_TRUNCATE);
    if(!rogue_texture_load(&slot->tex, path)) { slot->path[0]='\0'; return NULL; }
    g_sheet_tex_count++;
    return &slot->tex;
}

static void veg_render_instance(RogueVegetationInstance* v){
    RogueVegetationDef* d=&g_defs[v->def_index];
    RogueTexture* tex = veg_get_texture(d->image);
    if(!tex || !tex->handle) return; /* fallback silently */
    int tiles_w = (int)(d->tile_x2 - d->tile_x + 1);
    int tiles_h = (int)(d->tile_y2 - d->tile_y + 1);
    int sprite_px_w = tiles_w * g_app.tile_size;
    int sprite_px_h = tiles_h * g_app.tile_size;
    /* Source rectangle in sheet uses tile coordinates scaled by tile_size */
    SDL_Rect src = { (int)d->tile_x * g_app.tile_size, (int)d->tile_y * g_app.tile_size, sprite_px_w, sprite_px_h };
    /* Destination: anchor bottom-center at vegetation (x,y) world position */
    int world_px_x = (int)((v->x - g_app.cam_x) * g_app.tile_size);
    int world_px_y = (int)((v->y - g_app.cam_y) * g_app.tile_size);
    SDL_Rect dst = { world_px_x - sprite_px_w/2, world_px_y - sprite_px_h, sprite_px_w, sprite_px_h };
    SDL_RenderCopy(g_app.renderer, tex->handle, &src, &dst);
}
#endif

void rogue_vegetation_render(void){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;
    for(int i=0;i<g_instance_count;i++) veg_render_instance(&g_instances[i]);
#endif
}

int rogue_vegetation_count(void){ return g_instance_count; }
int rogue_vegetation_tree_count(void){ int c=0; for(int i=0;i<g_instance_count;i++) if(g_instances[i].is_tree) c++; return c; }
int rogue_vegetation_plant_count(void){ int c=0; for(int i=0;i<g_instance_count;i++) if(!g_instances[i].is_tree) c++; return c; }

int rogue_vegetation_tile_blocking(int tx,int ty){
    float fx=tx+0.5f, fy=ty+0.5f; for(int i=0;i<g_instance_count;i++){ if(g_instances[i].is_tree){ float dx=g_instances[i].x-fx; float dy=g_instances[i].y-fy; if(fabsf(dx)<0.51f && fabsf(dy)<0.51f) return 1; }} return 0;
}
float rogue_vegetation_tile_move_scale(int tx,int ty){
    float fx=tx+0.5f, fy=ty+0.5f; for(int i=0;i<g_instance_count;i++){ if(!g_instances[i].is_tree){ float dx=g_instances[i].x-fx; float dy=g_instances[i].y-fy; if(fabsf(dx)<0.51f && fabsf(dy)<0.51f) return 0.85f; }} return 1.0f;
}
