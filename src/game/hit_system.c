#include "game/hit_system.h"
#include "util/log.h"
#include "core/app_state.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define MAX_HIT_GEO 16
static RogueWeaponHitGeo g_hit_geo[MAX_HIT_GEO];
static int g_hit_geo_count = 0;

int g_hit_debug_enabled = 0;
static RogueHitDebugFrame g_last_debug = {0};
static RogueHitboxTuning g_tuning = {0};
static char g_tuning_path[260];

const char* rogue_hitbox_tuning_path(void){ return g_tuning_path[0]? g_tuning_path : NULL; }

int rogue_hitbox_tuning_resolve_path(void){
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
        if(klen==15 && strncmp(key,"player_offset_x",15)==0) out->player_offset_x=fv; else if(klen==15 && strncmp(key,"player_offset_y",15)==0) out->player_offset_y=fv; else if(klen==13 && strncmp(key,"player_length",13)==0) out->player_length=fv; else if(klen==12 && strncmp(key,"player_width",12)==0) out->player_width=fv; else if(klen==12 && strncmp(key,"enemy_radius",12)==0) out->enemy_radius=fv; else if(klen==14 && strncmp(key,"enemy_offset_x",14)==0) out->enemy_offset_x=fv; else if(klen==14 && strncmp(key,"enemy_offset_y",14)==0) out->enemy_offset_y=fv; if(*s==',') s++; }
    free(buf); return 0; }

int rogue_hitbox_tuning_save(const char* path, const RogueHitboxTuning* t){ if(!path||!t) return -1; FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,path,"wb");
#else
    f=fopen(path,"wb");
#endif
    if(!f) return -1; fprintf(f,"{\n  \"player_offset_x\": %.4f,\n  \"player_offset_y\": %.4f,\n  \"player_length\": %.4f,\n  \"player_width\": %.4f,\n  \"enemy_radius\": %.4f,\n  \"enemy_offset_x\": %.4f,\n  \"enemy_offset_y\": %.4f\n}\n", t->player_offset_x,t->player_offset_y,t->player_length,t->player_width,t->enemy_radius,t->enemy_offset_x,t->enemy_offset_y); fclose(f); ROGUE_LOG_INFO("hitbox_tuning_saved: %s", path); return 0; }

const RogueHitDebugFrame* rogue_hit_debug_last(void){ return &g_last_debug; }
void rogue_hit_debug_store(const RogueCapsule* c, int* indices, float (*normals)[2], int hit_count, int frame_id){
    if(c){ g_last_debug.last_capsule = *c; g_last_debug.capsule_valid=1; }
    g_last_debug.hit_count = (hit_count>32)?32:hit_count; for(int i=0;i<g_last_debug.hit_count;i++){ g_last_debug.last_hits[i]=indices[i]; if(normals){ g_last_debug.normals[i][0]=normals[i][0]; g_last_debug.normals[i][1]=normals[i][1]; } }
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
static inline int test_and_set_hit(int idx){ int byte=idx>>3; int bit=1<<(idx&7); int was = g_sweep_hit_mask[byte] & bit; g_sweep_hit_mask[byte]|=bit; return was!=0; }
static inline void reset_hit_mask(void){ memset(g_sweep_hit_mask,0,sizeof g_sweep_hit_mask); }
void rogue_hit_sweep_reset(void){ reset_hit_mask(); }
/* Access last sweep list */
int rogue_hit_last_indices(const int** out_indices){ if(out_indices) *out_indices = g_last_indices; return g_last_count; }

/* Integration temporary: performing overlap *and* letting combat_strike still do damage logic; we only gate which enemies allowed (replace reach). */
/* Compute closest point on segment to p; return squared distance and out normal from segment to point (normalized) */
static float closest_point_seg(float x0,float y0,float x1,float y1,float px,float py,float* outx,float* outy,float* nx,float* ny){
    float vx=x1-x0, vy=y1-y0; float wx=px-x0, wy=py-y0; float vv=vx*vx+vy*vy; float t=0.0f; if(vv>0){ t=(vx*wx+vy*wy)/vv; if(t<0) t=0; else if(t>1) t=1; }
    *outx = x0 + vx*t; *outy = y0 + vy*t; float dx=px-*outx, dy=py-*outy; float d2=dx*dx+dy*dy; float len = (float)sqrt(d2); if(len>0){ *nx = dx/len; *ny = dy/len; } else { *nx=0; *ny=1; }
    return d2;
}

int rogue_combat_weapon_sweep_apply(struct RoguePlayerCombat* pc, struct RoguePlayer* player, struct RogueEnemy enemies[], int enemy_count){
    if(!pc||!player||!enemies) return 0; if(pc->phase != ROGUE_ATTACK_STRIKE) return 0; const RogueWeaponHitGeo* geo = rogue_weapon_hit_geo_get(player->equipped_weapon_id); if(!geo){ rogue_weapon_hit_geo_ensure_default(); geo = rogue_weapon_hit_geo_get(player->equipped_weapon_id); }
    RogueCapsule cap; if(!rogue_weapon_build_capsule(player,geo,&cap)) return 0; int hit_indices[32]; float normals[32][2]; int hc=0; float cap_aabb_xmin = fminf(cap.x0,cap.x1) - cap.r; float cap_aabb_xmax = fmaxf(cap.x0,cap.x1) + cap.r; float cap_aabb_ymin = fminf(cap.y0,cap.y1) - cap.r; float cap_aabb_ymax = fmaxf(cap.y0,cap.y1) + cap.r;
    float enemy_r = (g_tuning.enemy_radius>0)? g_tuning.enemy_radius : 0.40f;
    for(int i=0;i<enemy_count;i++){ if(!enemies[i].alive) continue; if(test_and_set_hit(i)) continue; float ex=enemies[i].base.pos.x + g_tuning.enemy_offset_x; float ey=enemies[i].base.pos.y + g_tuning.enemy_offset_y; /* treat enemy as circle */
        if(ex < cap_aabb_xmin-0.6f || ex > cap_aabb_xmax+0.6f || ey < cap_aabb_ymin-0.6f || ey > cap_aabb_ymax+0.6f) continue; /* broadphase reject */
        float cx,cy,nx,ny; float d2 = closest_point_seg(cap.x0,cap.y0,cap.x1,cap.y1,ex,ey,&cx,&cy,&nx,&ny); float cr=enemy_r; float rr = cr + cap.r; if(d2 <= rr*rr){ hit_indices[hc]=i; normals[hc][0]=nx; normals[hc][1]=ny; hc++; if(hc>=32) break; }
    }
    g_last_count = hc; for(int k=0;k<hc;k++) g_last_indices[k]=hit_indices[k];
    rogue_hit_debug_store(&cap, hit_indices, normals, hc, g_app.frame_count);
    return hc;
}
