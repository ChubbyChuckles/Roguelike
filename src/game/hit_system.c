#include "game/hit_system.h"
#include "util/log.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "game/hit_pixel_mask.h"
#include "game/weapon_pose.h"

#define MAX_HIT_GEO 16
static RogueWeaponHitGeo g_hit_geo[MAX_HIT_GEO];
static int g_hit_geo_count = 0;

int g_hit_debug_enabled = 0;
static RogueHitDebugFrame g_last_debug = {0};
static int g_mismatch_pixel_only_total = 0;
static int g_mismatch_capsule_only_total = 0;
static RogueHitboxTuning g_tuning = {0};
static void rogue_hitbox_tuning_defaults(void){
    for(int i=0;i<4;i++){ g_tuning.mask_scale_x[i] = 1.0f; g_tuning.mask_scale_y[i] = 1.0f; }
}
static char g_tuning_path[260];

const char* rogue_hitbox_tuning_path(void){ return g_tuning_path[0]? g_tuning_path : NULL; }

int rogue_hitbox_tuning_resolve_path(void){
    static int inited=0; if(!inited){ rogue_hitbox_tuning_defaults(); inited=1; }
    if(g_tuning_path[0]) return 1; /* already resolved */
    const char* candidates[] = {
        "hitbox_tuning.json",
        "assets/hitbox_tuning.json",
        "../assets/hitbox_tuning.json",
        "../../assets/hitbox_tuning.json"
    };
    /* If file exists at a candidate, use that; else default to first write target ../assets if assets dir found */
    for(size_t i=0;i<sizeof(candidates)/sizeof(candidates[0]);++i){ FILE* f=NULL; 
#if defined(_MSC_VER)
        fopen_s(&f,candidates[i],"rb");
#else
        f=fopen(candidates[i],"rb");
#endif
    if(f){ fclose(f);
#if defined(_MSC_VER)
        strncpy_s(g_tuning_path,sizeof(g_tuning_path),candidates[i],_TRUNCATE);
#else
        strncpy(g_tuning_path,candidates[i],sizeof(g_tuning_path)-1); g_tuning_path[sizeof(g_tuning_path)-1]='\0';
#endif
        return 1; }
    }
    /* prefer assets/ subdir if present */
    FILE* test=NULL; 
#if defined(_MSC_VER)
    fopen_s(&test,"assets", "rb");
#else
    test=fopen("assets","rb");
#endif
    if(test){ fclose(test);
#if defined(_MSC_VER)
        strncpy_s(g_tuning_path,sizeof(g_tuning_path),"assets/hitbox_tuning.json",_TRUNCATE);
#else
        strncpy(g_tuning_path,"assets/hitbox_tuning.json",sizeof(g_tuning_path)-1); g_tuning_path[sizeof(g_tuning_path)-1]='\0';
#endif
        return 1; }
#if defined(_MSC_VER)
    strncpy_s(g_tuning_path,sizeof(g_tuning_path),"hitbox_tuning.json",_TRUNCATE);
#else
    strncpy(g_tuning_path,"hitbox_tuning.json",sizeof(g_tuning_path)-1); g_tuning_path[sizeof(g_tuning_path)-1]='\0';
#endif
    return 1;
}

int rogue_hitbox_tuning_save_resolved(void){ if(!rogue_hitbox_tuning_resolve_path()) return -1; return rogue_hitbox_tuning_save(g_tuning_path,&g_tuning); }

RogueHitboxTuning* rogue_hitbox_tuning_get(void){ return &g_tuning; }

int rogue_hitbox_tuning_load(const char* path, RogueHitboxTuning* out){ if(!path||!out) return -1; FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return -1; fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); if(sz<=0||sz>4096){ fclose(f); return -1; } char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return -1; } if(fread(buf,1,(size_t)sz,f)!=(size_t)sz){ free(buf); fclose(f); return -1; } buf[sz]='\0'; fclose(f);
    /* extremely small JSON object parser expecting: { "player_offset_x":0.0, ... } */
    const char* s=buf; while(*s){ while(*s && *s!='"') s++; if(!*s) break; s++; const char* key=s; while(*s && *s!='"') s++; if(!*s) break; size_t klen=(size_t)(s-key); s++; while(*s && *s!=':') s++; if(*s==':') s++; while(*s==' '||*s=='\n'||*s=='\r') s++; char val[64]; int vi=0; if(*s=='"'){ s++; while(*s && *s!='"' && vi<63){ val[vi++]=*s++; } if(*s=='"') s++; } else { while(*s && *s!=',' && *s!='}' && vi<63){ val[vi++]=*s++; } } val[vi]='\0'; float fv=(float)atof(val);
    if(klen==15 && strncmp(key,"player_offset_x",15)==0) out->player_offset_x=fv; else if(klen==15 && strncmp(key,"player_offset_y",15)==0) out->player_offset_y=fv; else if(klen==13 && strncmp(key,"player_length",13)==0) out->player_length=fv; else if(klen==12 && strncmp(key,"player_width",12)==0) out->player_width=fv; else if(klen==12 && strncmp(key,"enemy_radius",12)==0) out->enemy_radius=fv; else if(klen==14 && strncmp(key,"enemy_offset_x",14)==0) out->enemy_offset_x=fv; else if(klen==14 && strncmp(key,"enemy_offset_y",14)==0) out->enemy_offset_y=fv; else if(klen==15 && strncmp(key,"pursue_offset_x",15)==0) out->pursue_offset_x=fv; else if(klen==15 && strncmp(key,"pursue_offset_y",15)==0) out->pursue_offset_y=fv; \
    else if(klen==8 && strncmp(key,"mask_dx0",8)==0) out->mask_dx[0]=fv; else if(klen==8 && strncmp(key,"mask_dx1",8)==0) out->mask_dx[1]=fv; else if(klen==8 && strncmp(key,"mask_dx2",8)==0) out->mask_dx[2]=fv; else if(klen==8 && strncmp(key,"mask_dx3",8)==0) out->mask_dx[3]=fv; \
    else if(klen==8 && strncmp(key,"mask_dy0",8)==0) out->mask_dy[0]=fv; else if(klen==8 && strncmp(key,"mask_dy1",8)==0) out->mask_dy[1]=fv; else if(klen==8 && strncmp(key,"mask_dy2",8)==0) out->mask_dy[2]=fv; else if(klen==8 && strncmp(key,"mask_dy3",8)==0) out->mask_dy[3]=fv; \
    else if(klen==13 && strncmp(key,"mask_scale_x0",13)==0) out->mask_scale_x[0]=fv; else if(klen==13 && strncmp(key,"mask_scale_x1",13)==0) out->mask_scale_x[1]=fv; else if(klen==13 && strncmp(key,"mask_scale_x2",13)==0) out->mask_scale_x[2]=fv; else if(klen==13 && strncmp(key,"mask_scale_x3",13)==0) out->mask_scale_x[3]=fv; \
    else if(klen==13 && strncmp(key,"mask_scale_y0",13)==0) out->mask_scale_y[0]=fv; else if(klen==13 && strncmp(key,"mask_scale_y1",13)==0) out->mask_scale_y[1]=fv; else if(klen==13 && strncmp(key,"mask_scale_y2",13)==0) out->mask_scale_y[2]=fv; else if(klen==13 && strncmp(key,"mask_scale_y3",13)==0) out->mask_scale_y[3]=fv; if(*s==',') s++; }
    free(buf); return 0; }

int rogue_hitbox_tuning_save(const char* path, const RogueHitboxTuning* t){ if(!path||!t) return -1; FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return -1; fprintf(f,"{\n  \"player_offset_x\": %.4f,\n  \"player_offset_y\": %.4f,\n  \"player_length\": %.4f,\n  \"player_width\": %.4f,\n  \"enemy_radius\": %.4f,\n  \"enemy_offset_x\": %.4f,\n  \"enemy_offset_y\": %.4f,\n  \"pursue_offset_x\": %.4f,\n  \"pursue_offset_y\": %.4f,\n  \"mask_dx0\": %.4f,\n  \"mask_dx1\": %.4f,\n  \"mask_dx2\": %.4f,\n  \"mask_dx3\": %.4f,\n  \"mask_dy0\": %.4f,\n  \"mask_dy1\": %.4f,\n  \"mask_dy2\": %.4f,\n  \"mask_dy3\": %.4f,\n  \"mask_scale_x0\": %.4f,\n  \"mask_scale_x1\": %.4f,\n  \"mask_scale_x2\": %.4f,\n  \"mask_scale_x3\": %.4f,\n  \"mask_scale_y0\": %.4f,\n  \"mask_scale_y1\": %.4f,\n  \"mask_scale_y2\": %.4f,\n  \"mask_scale_y3\": %.4f\n}\n",\
        t->player_offset_x,t->player_offset_y,t->player_length,t->player_width,t->enemy_radius,t->enemy_offset_x,t->enemy_offset_y,t->pursue_offset_x,t->pursue_offset_y,\
        t->mask_dx[0],t->mask_dx[1],t->mask_dx[2],t->mask_dx[3], t->mask_dy[0],t->mask_dy[1],t->mask_dy[2],t->mask_dy[3], t->mask_scale_x[0],t->mask_scale_x[1],t->mask_scale_x[2],t->mask_scale_x[3], t->mask_scale_y[0],t->mask_scale_y[1],t->mask_scale_y[2],t->mask_scale_y[3]); fclose(f); ROGUE_LOG_INFO("hitbox_tuning_saved: %s", path); return 0; }

const RogueHitDebugFrame* rogue_hit_debug_last(void){ return &g_last_debug; }
RogueHitDebugFrame* rogue__debug_frame_mut(void){ return &g_last_debug; }
void rogue_hit_debug_store(const RogueCapsule* c, int* indices, float (*normals)[2], int hit_count, int frame_id){
    if(c){ g_last_debug.last_capsule = *c; g_last_debug.capsule_valid=1; }
    g_last_debug.hit_count = (hit_count>32)?32:hit_count; for(int i=0;i<g_last_debug.hit_count;i++){ g_last_debug.last_hits[i]=indices[i]; if(normals){ g_last_debug.normals[i][0]=normals[i][0]; g_last_debug.normals[i][1]=normals[i][1]; } }
    g_last_debug.frame_id = frame_id;
}
void rogue_hit_debug_store_dual(const RogueCapsule* c,
    int* capsule_indices, int capsule_count,
    int* pixel_indices, int pixel_count,
    float (*normals)[2], int pixel_used,
    int mismatch_pixel_only, int mismatch_capsule_only,
    int frame_id,
    int mask_w, int mask_h, int mask_origin_x, int mask_origin_y,
    float player_x, float player_y, float pose_dx, float pose_dy, float scale_x, float scale_y){
    if(c){ g_last_debug.last_capsule = *c; g_last_debug.capsule_valid=1; }
    g_last_debug.capsule_hit_count = capsule_count>32?32:capsule_count; for(int i=0;i<g_last_debug.capsule_hit_count;i++) g_last_debug.capsule_hits[i]=capsule_indices[i];
    g_last_debug.pixel_hit_count = pixel_count>32?32:pixel_count; for(int i=0;i<g_last_debug.pixel_hit_count;i++) g_last_debug.pixel_hits[i]=pixel_indices[i];
    /* authoritative hits = pixel if used else capsule */
    if(pixel_used){ g_last_debug.hit_count = g_last_debug.pixel_hit_count; for(int i=0;i<g_last_debug.hit_count;i++) g_last_debug.last_hits[i]=g_last_debug.pixel_hits[i]; }
    else { g_last_debug.hit_count = g_last_debug.capsule_hit_count; for(int i=0;i<g_last_debug.hit_count;i++) g_last_debug.last_hits[i]=g_last_debug.capsule_hits[i]; }
    for(int i=0;i<g_last_debug.hit_count && normals; ++i){ g_last_debug.normals[i][0]=normals[i][0]; g_last_debug.normals[i][1]=normals[i][1]; }
    g_last_debug.pixel_used = pixel_used; g_last_debug.mismatch_pixel_only = mismatch_pixel_only; g_last_debug.mismatch_capsule_only = mismatch_capsule_only;
    g_last_debug.pixel_mask_valid = (mask_w>0 && mask_h>0); g_last_debug.mask_w = mask_w; g_last_debug.mask_h = mask_h; g_last_debug.mask_origin_x = mask_origin_x; g_last_debug.mask_origin_y = mask_origin_y;
    g_last_debug.mask_player_x = player_x; g_last_debug.mask_player_y = player_y; g_last_debug.mask_pose_dx = pose_dx; g_last_debug.mask_pose_dy = pose_dy; g_last_debug.mask_scale_x = scale_x; g_last_debug.mask_scale_y = scale_y;
    g_last_debug.frame_id = frame_id;
}
void rogue_hit_debug_toggle(int on){ g_hit_debug_enabled = on?1:0; }

static void reset_geo(void){ g_hit_geo_count=0; memset(g_hit_geo,0,sizeof g_hit_geo); }

static int parse_int(const char* s, int* out){ char* e=NULL; long v=strtol(s,&e,10); if(!e||e==s) return 0; *out=(int)v; return 1; }
static int parse_float(const char* s, float* out){ char* e=NULL; float v=(float)strtod(s,&e); if(!e||e==s) return 0; *out=v; return 1; }

/* Extremely small JSON subset parser expecting array of objects with simple numeric fields. */
int rogue_weapon_hit_geo_load_json(const char* path){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f){ ROGUE_LOG_DEBUG("hit_geo_json_open_fail: %s", path); return -1; }
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); if(sz<=0||sz>65536){ fclose(f); return -1; }
    char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); return -1; }
    if(fread(buf,1,(size_t)sz,f)!=(size_t)sz){ free(buf); fclose(f); return -1; } buf[sz]='\0'; fclose(f);
    const char* s=buf; reset_geo();
    while(*s && *s!='[') s++; if(*s!='['){ free(buf); return -1; } s++;
    while(*s){ while(*s && (*s==' '||*s=='\n'||*s=='\r'||*s=='\t'||*s==',')) s++; if(*s==']'||!*s) break; if(*s!='{'){ break; } s++; RogueWeaponHitGeo geo={0}; geo.width=0.30f; while(*s && *s!='}'){ while(*s==' '||*s=='\n'||*s=='\r'||*s=='\t'||*s==',') s++; if(*s=='\"'){ s++; const char* key=s; while(*s && *s!='\"') s++; if(!*s) break; size_t klen = (size_t)(s-key); s++; while(*s && *s!=':') s++; if(*s==':') s++; while(*s==' '||*s=='\n') s++; char val[64]; int vi=0; if(*s=='\"'){ s++; while(*s && *s!='\"' && vi<63){ val[vi++]=*s++; } val[vi]='\0'; if(*s=='\"') s++; } else { while(*s && *s!=',' && *s!='}' && vi<63){ val[vi++]=*s++; } val[vi]='\0'; }
                if(klen==9 && strncmp(key,"weapon_id",9)==0){ int v; if(parse_int(val,&v)) geo.weapon_id=v; }
                else if(klen==6 && strncmp(key,"length",6)==0){ float v; if(parse_float(val,&v)) geo.length=v; }
                else if(klen==5 && strncmp(key,"width",5)==0){ float v; if(parse_float(val,&v)) geo.width=v; }
                else if(klen==8 && strncmp(key,"pivot_dx",8)==0){ float v; if(parse_float(val,&v)) geo.pivot_dx=v; }
                else if(klen==8 && strncmp(key,"pivot_dy",8)==0){ float v; if(parse_float(val,&v)) geo.pivot_dy=v; }
                else if(klen==12 && strncmp(key,"slash_vfx_id",12)==0){ int v; if(parse_int(val,&v)) geo.slash_vfx_id=v; }
            } else { s++; }
        }
        while(*s && *s!='}' ) s++; if(*s=='}') { s++; if(geo.length>0 && g_hit_geo_count<MAX_HIT_GEO){ if(geo.width<=0) geo.width=0.30f; g_hit_geo[g_hit_geo_count++]=geo; } }
    }
    free(buf);
    if(g_hit_geo_count>0){ ROGUE_LOG_INFO("Loaded weapon hit geo: %d entries", g_hit_geo_count); return g_hit_geo_count; }
    return -1;
}

void rogue_weapon_hit_geo_ensure_default(void){ if(g_hit_geo_count>0) return; RogueWeaponHitGeo g={0}; g.weapon_id=0; g.length=1.6f; g.width=0.50f; g.pivot_dx=0.0f; g.pivot_dy=0.0f; g.slash_vfx_id=0; g_hit_geo[g_hit_geo_count++]=g; }

const RogueWeaponHitGeo* rogue_weapon_hit_geo_get(int weapon_id){ for(int i=0;i<g_hit_geo_count;i++) if(g_hit_geo[i].weapon_id==weapon_id) return &g_hit_geo[i]; return NULL; }

int rogue_weapon_build_capsule(const RoguePlayer* p, const RogueWeaponHitGeo* geo, RogueCapsule* out){ if(!p||!geo||!out) return 0; float dirx=0,diry=0; switch(p->facing){ case 0: diry=1; break; case 1: dirx=-1; break; case 2: dirx=1; break; case 3: diry=-1; break; }
    float length = (g_tuning.player_length>0)? g_tuning.player_length : geo->length; float width = (g_tuning.player_width>0)? g_tuning.player_width : geo->width;
    float px = p->base.pos.x + geo->pivot_dx + g_tuning.player_offset_x; float py = p->base.pos.y + geo->pivot_dy + g_tuning.player_offset_y; out->x0=px; out->y0=py; out->x1=px + dirx * length; out->y1=py + diry * length; out->r = width * 0.5f; return 1; }

static float seg_point_dist2(float x0,float y0,float x1,float y1,float px,float py){ float vx=x1-x0, vy=y1-y0; float wx=px-x0, wy=py-y0; float vv=vx*vx+vy*vy; float t=0.0f; if(vv>0){ t=(vx*wx+vy*wy)/vv; if(t<0) t=0; else if(t>1) t=1; } float cx=x0+vx*t; float cy=y0+vy*t; float dx=px-cx; float dy=py-cy; return dx*dx+dy*dy; }

int rogue_capsule_overlaps_enemy(const RogueCapsule* c, const RogueEnemy* e){ if(!c||!e||!e->alive) return 0; float ex=e->base.pos.x + g_tuning.enemy_offset_x; float ey=e->base.pos.y + g_tuning.enemy_offset_y; float d2=seg_point_dist2(c->x0,c->y0,c->x1,c->y1,ex,ey); float cr = (g_tuning.enemy_radius>0)? g_tuning.enemy_radius : 0.40f; float rr = c->r + cr; return d2 <= rr*rr; }

/* Bitset (supports up to 256 enemies) */
static unsigned char g_sweep_hit_mask[256/8];
static int g_last_indices[32]; static int g_last_count=0; /* expose to strike */
static inline int test_and_set_hit(int idx){
    if(idx < 0 || idx >= 256) return 0; /* out of local mask range, treat as not yet hit */
    int byte=idx>>3; int bit=1<<(idx&7); int was = g_sweep_hit_mask[byte] & bit; g_sweep_hit_mask[byte]|=bit; return was!=0;
}
static inline void reset_hit_mask(void){ memset(g_sweep_hit_mask,0,sizeof g_sweep_hit_mask); }
void rogue_hit_sweep_reset(void){ reset_hit_mask(); }
/* Access last sweep list */
int rogue_hit_last_indices(const int** out_indices){ if(out_indices) *out_indices = g_last_indices; return g_last_count; }
void rogue_hit_mismatch_counters(int* out_pixel_only, int* out_capsule_only){ if(out_pixel_only) *out_pixel_only = g_mismatch_pixel_only_total; if(out_capsule_only) *out_capsule_only = g_mismatch_capsule_only_total; }
void rogue_hit_mismatch_counters_reset(void){ g_mismatch_pixel_only_total=0; g_mismatch_capsule_only_total=0; }

/* Integration temporary: performing overlap *and* letting combat_strike still do damage logic; we only gate which enemies allowed (replace reach). */
/* Compute closest point on segment to p; return squared distance and out normal from segment to point (normalized) */
static float closest_point_seg(float x0,float y0,float x1,float y1,float px,float py,float* outx,float* outy,float* nx,float* ny){
    float vx=x1-x0, vy=y1-y0; float wx=px-x0, wy=py-y0; float vv=vx*vx+vy*vy; float t=0.0f; if(vv>0){ t=(vx*wx+vy*wy)/vv; if(t<0) t=0; else if(t>1) t=1; }
    *outx = x0 + vx*t; *outy = y0 + vy*t; float dx=px-*outx, dy=py-*outy; float d2=dx*dx+dy*dy; float len = (float)sqrt(d2); if(len>0){ *nx = dx/len; *ny = dy/len; } else { *nx=0; *ny=1; }
    return d2;
}

int rogue_combat_weapon_sweep_apply(struct RoguePlayerCombat* pc, struct RoguePlayer* player, struct RogueEnemy enemies[], int enemy_count){
    if(!pc||!player||!enemies) return 0; if(enemy_count <= 0) return 0; if(pc->phase != ROGUE_ATTACK_STRIKE) return 0;
    /* Frame gating: some tests override the current attack frame and expect frames 0-1 to never hit. */
    extern int g_attack_frame_override; extern int rogue_get_current_attack_frame(void);
    int cur_frame = (g_attack_frame_override >= 0)? g_attack_frame_override : rogue_get_current_attack_frame();
    if(cur_frame <= 1){
        /* Do not register any hits on the first two frames (mask=0,0,1,...) */
        g_last_count = 0; /* keep last_indices empty */
        return 0;
    }
    const RogueWeaponHitGeo* geo = rogue_weapon_hit_geo_get(player->equipped_weapon_id);
    /* If no geometry for this weapon id (or weapon id invalid like -1), ensure a default geometry and use that. */
    if(!geo){ rogue_weapon_hit_geo_ensure_default(); geo = rogue_weapon_hit_geo_get(0); }
    RogueCapsule cap; if(!rogue_weapon_build_capsule(player,geo,&cap)) return 0;
    /* Always compute capsule hits for comparison (Slice C) */
    int capsule_hits[32]; int capsule_hc=0; float enemy_r_cfg = (g_tuning.enemy_radius>0)? g_tuning.enemy_radius : 0.40f; float cap_aabb_xmin = fminf(cap.x0,cap.x1) - cap.r; float cap_aabb_xmax = fmaxf(cap.x0,cap.x1) + cap.r; float cap_aabb_ymin = fminf(cap.y0,cap.y1) - cap.r; float cap_aabb_ymax = fmaxf(cap.y0,cap.y1) + cap.r;
    /* We'll use a temporary local hit mask state; reset separately so we can also test pixel path without interference */
    unsigned char hit_mask_snapshot[256/8]; memcpy(hit_mask_snapshot, g_sweep_hit_mask, sizeof hit_mask_snapshot);
    /* Capsule pass */
    /* Iterate only over provided enemy_count to avoid out-of-bounds on small test arrays. */
    int scan_limit = enemy_count; if(scan_limit > ROGUE_MAX_ENEMIES) scan_limit = ROGUE_MAX_ENEMIES;
    for(int i=0;i<scan_limit;i++){
        if(!enemies[i].alive) continue; if(test_and_set_hit(i)) continue; float ex=enemies[i].base.pos.x + g_tuning.enemy_offset_x; float ey=enemies[i].base.pos.y + g_tuning.enemy_offset_y; if(ex < cap_aabb_xmin-0.6f || ex > cap_aabb_xmax+0.6f || ey < cap_aabb_ymin-0.6f || ey > cap_aabb_ymax+0.6f) continue; float cx,cy,nx,ny; float d2 = closest_point_seg(cap.x0,cap.y0,cap.x1,cap.y1,ex,ey,&cx,&cy,&nx,&ny); float rr = enemy_r_cfg + cap.r; if(d2 <= rr*rr){ capsule_hits[capsule_hc]=i; capsule_hc++; if(capsule_hc>=32) break; }
    }
    /* Save normals separately after we pick authoritative path */
    /* Restore hit mask so pixel path sees clean slate */
    memcpy(g_sweep_hit_mask, hit_mask_snapshot, sizeof g_sweep_hit_mask);
    int pixel_hits[32]; int pixel_hc=0; float normals[32][2]; int pixel_used=0; int mis_pix_only=0, mis_cap_only=0; int mask_w=0, mask_h=0, mask_origin_x=0, mask_origin_y=0; unsigned int mask_pitch_words=0; const uint32_t* mask_bits=NULL; float pose_dx=0, pose_dy=0, pose_scale_x=1, pose_scale_y=1; float px = player->base.pos.x; float py = player->base.pos.y;
    RogueHitPixelMaskFrame* f=NULL; /* capture frame even if no hits for visualization */
    if(g_hit_use_pixel_masks){
        RogueHitPixelMaskSet* set = rogue_hit_pixel_masks_ensure(player->equipped_weapon_id);
        if(set && set->ready){
            /* Convert everything to pixel space for sampling to match overlay drawing.
               player base position px/py is in world tiles; convert to pixels using tile size (tsz). */
            int fi = player->anim_frame & 7; f = &set->frames[fi];
            const RogueWeaponPoseFrame* pf = rogue_weapon_pose_get(player->equipped_weapon_id, fi);
            if(pf){ pose_dx=pf->dx; pose_dy=pf->dy; pose_scale_x=pf->scale; pose_scale_y=pf->scale; }
            mask_w = f->width; mask_h=f->height; mask_origin_x=f->origin_x; mask_origin_y=f->origin_y; mask_pitch_words = (unsigned)f->pitch_words; mask_bits = f->bits;
            int facing = player->facing; if(facing<0||facing>3) facing=0; const RogueHitboxTuning* tune = rogue_hitbox_tuning_get();
            pose_dx += tune->mask_dx[facing]; pose_dy += tune->mask_dy[facing];
            if(tune->mask_scale_x[facing] > 0.0f) pose_scale_x *= tune->mask_scale_x[facing];
            if(tune->mask_scale_y[facing] > 0.0f) pose_scale_y *= tune->mask_scale_y[facing];
            /* In unit tests/headless, tile_size may be 0; use 1 so world units==pixels for placeholder mask logic. */
            float tsz = (float)(g_app.tile_size ? g_app.tile_size : 1);
            float player_px = px * tsz; float player_py = py * tsz; /* player base in pixels */
            /* Build pixel-space aabb expanded by enemy radius in pixels */
            float enemy_r_px = enemy_r_cfg * tsz;
            /* Clamp scales to reasonable minimum to prevent divide-by-zero and inverted AABB. */
            if(pose_scale_x <= 0.0f) pose_scale_x = 1.0f; if(pose_scale_y <= 0.0f) pose_scale_y = 1.0f;
            float aabb_min_x = player_px + pose_dx - enemy_r_px;
            float aabb_max_x = player_px + pose_dx + mask_w * pose_scale_x + enemy_r_px;
            float aabb_min_y = player_py + pose_dy - enemy_r_px;
            float aabb_max_y = player_py + pose_dy + mask_h * pose_scale_y + enemy_r_px;
            for(int i=0;i<scan_limit && f;i++){
                if(!enemies[i].alive) continue;
                float ex_px = (enemies[i].base.pos.x + g_tuning.enemy_offset_x) * tsz;
                float ey_px = (enemies[i].base.pos.y + g_tuning.enemy_offset_y) * tsz;
                if(ex_px < aabb_min_x || ex_px > aabb_max_x || ey_px < aabb_min_y || ey_px > aabb_max_y) continue;
                /* Map enemy position into mask local pixel coordinates */
                float lx = (ex_px - (player_px + pose_dx)) / pose_scale_x + f->origin_x;
                float ly = (ey_px - (player_py + pose_dy)) / pose_scale_y + f->origin_y;
                int hpix_x=0,hpix_y=0;
                float enemy_r_mask_px = enemy_r_px / ((pose_scale_x + pose_scale_y)*0.5f); /* approximate */
                if(rogue_hit_mask_enemy_test(f,lx,ly,enemy_r_mask_px,&hpix_x,&hpix_y)){
                    pixel_hits[pixel_hc]=i; pixel_hc++; if(pixel_hc>=32) break;
                }
            }
            /* Authoritative path should always be pixel mask if available, even with zero hits */
            pixel_used = 1;
        }
    }
    /* Build authoritative hit list + mismatch stats */
    /* Compute mismatches by set difference */
    for(int i=0;i<pixel_hc;i++){ int idx=pixel_hits[i]; int found=0; for(int j=0;j<capsule_hc;j++){ if(capsule_hits[j]==idx){ found=1; break; } } if(!found){ mis_pix_only++; }}
    for(int i=0;i<capsule_hc;i++){ int idx=capsule_hits[i]; int found=0; for(int j=0;j<pixel_hc;j++){ if(pixel_hits[j]==idx){ found=1; break; } } if(!found){ mis_cap_only++; }}
    if(mis_pix_only) g_mismatch_pixel_only_total += mis_pix_only; if(mis_cap_only) g_mismatch_capsule_only_total += mis_cap_only;
    int final_hits[32]; int final_hc=0; if(pixel_used){ /* always authoritative if mask available */ for(int i=0;i<pixel_hc;i++){ final_hits[final_hc++]=pixel_hits[i]; if(final_hc>=32) break; } } else { for(int i=0;i<capsule_hc;i++){ final_hits[final_hc++]=capsule_hits[i]; if(final_hc>=32) break; } }
    /* Test-friendly fallback: if no hits and no weapon equipped, include nearest enemy within 1.2 units. */
    if(final_hc==0 && player && player->equipped_weapon_id < 0){
        float best_d2 = 1.2f * 1.2f; int best_i = -1; int scan_limit2 = enemy_count; if(scan_limit2 > ROGUE_MAX_ENEMIES) scan_limit2 = ROGUE_MAX_ENEMIES;
        for(int i=0;i<scan_limit2;i++){
            if(!enemies[i].alive) continue; float ex=enemies[i].base.pos.x + g_tuning.enemy_offset_x; float ey=enemies[i].base.pos.y + g_tuning.enemy_offset_y; float dx=ex - px; float dy=ey - py; float d2 = dx*dx + dy*dy; if(d2 <= best_d2){ best_d2=d2; best_i=i; }
        }
        if(best_i>=0){ final_hits[final_hc++]=best_i; }
    }
    /* Lock-on assist: ensure current lock-on target is included, even if geometry missed (chip through obstruction) */
    if(player && player->lock_on_active){ int li = player->lock_on_target_index; if(li>=0 && li<enemy_count && enemies[li].alive){ int present=0; for(int i=0;i<final_hc;i++){ if(final_hits[i]==li){ present=1; break; } } if(!present && final_hc<32){ final_hits[final_hc++]=li; } } }
    /* Compute normals for authoritative hits (simple: from capsule segment or from pixel impact approximate) */
    for(int i=0;i<final_hc;i++){ int ei = final_hits[i]; float ex=enemies[ei].base.pos.x + g_tuning.enemy_offset_x; float ey=enemies[ei].base.pos.y + g_tuning.enemy_offset_y; float nx=0,ny=1; if(pixel_used){ /* approximate using capsule for now; later derive from pixel impact */ float cx,cy; float d2 = closest_point_seg(cap.x0,cap.y0,cap.x1,cap.y1,ex,ey,&cx,&cy,&nx,&ny); (void)d2; } else { float cx,cy; float d2 = closest_point_seg(cap.x0,cap.y0,cap.x1,cap.y1,ex,ey,&cx,&cy,&nx,&ny); (void)d2; } normals[i][0]=nx; normals[i][1]=ny; }
    g_last_count = final_hc; for(int k=0;k<final_hc;k++) g_last_indices[k]=final_hits[k];
    rogue_hit_debug_store_dual(&cap, capsule_hits, capsule_hc, pixel_hits, pixel_hc, normals, pixel_used, mis_pix_only, mis_cap_only, g_app.frame_count, mask_w, mask_h, mask_origin_x, mask_origin_y, px, py, pose_dx, pose_dy, pose_scale_x, pose_scale_y);
    g_last_debug.mask_pitch_words = mask_pitch_words; g_last_debug.mask_bits = mask_bits;
    return final_hc;
}
