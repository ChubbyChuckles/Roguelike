#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "player_assets.h"
#include "graphics/sprite.h"
#include "graphics/font.h"
#include "graphics/tile_sprites.h"
#include "entities/player.h"
#include "game/combat.h"
#include "util/log.h"

static int state_name_to_index(const char* s){ if(strcmp(s,"idle")==0) return 0; if(strcmp(s,"walk")==0) return 1; if(strcmp(s,"run")==0) return 2; if(strcmp(s,"attack")==0) return 3; return -1; }
static int dir_name_to_index(const char* d){ if(strcmp(d,"down")==0) return 0; if(strcmp(d,"left")==0) return 1; if(strcmp(d,"right")==0) return 2; if(strcmp(d,"side")==0) return 1; if(strcmp(d,"up")==0) return 3; return -1; }

static void load_player_sheet_paths(const char* path){
    FILE* f=NULL; int lines=0,loaded=0;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f){
        const char* prefixes[] = { "../", "../../", "../../../" };
        char attempt[512];
        for(size_t i=0;i<sizeof(prefixes)/sizeof(prefixes[0]) && !f;i++){
            snprintf(attempt,sizeof attempt,"%s%s", prefixes[i], path);
#if defined(_MSC_VER)
            fopen_s(&f,attempt,"rb");
#else
            f=fopen(attempt,"rb");
#endif
            if(f){ ROGUE_LOG_INFO("Opened player sheet config via fallback path: %s", attempt); break; }
        }
    }
    if(!f){ ROGUE_LOG_WARN("player sheet config open failed: %s", path); return; }
    char line[512];
    while(fgets(line,sizeof line,f)){
        lines++;
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(*p=='#' || *p=='\n' || *p=='\0') continue;
        if(strncmp(p,"SHEET",5)!=0) continue;
        p+=5; if(*p==',') p++; while(*p==' '||*p=='\t') p++;
        char st_tok[32]; int si=0; while(p[si] && p[si]!=',' && p[si]!='\n' && si<31) { st_tok[si]=p[si]; si++; }
        if(p[si]!=',') continue; st_tok[si]='\0'; p += si+1; while(*p==' '||*p=='\t') p++;
        char dir_tok[32]; int di=0; while(p[di] && p[di]!=',' && p[di]!='\n' && di<31){ dir_tok[di]=p[di]; di++; }
        if(p[di]!=',') continue; dir_tok[di]='\0'; p += di+1; while(*p==' '||*p=='\t') p++;
        char path_tok[256]; int pi=0; while(p[pi] && p[pi]!='\n' && pi<255){ path_tok[pi]=p[pi]; pi++; }
        path_tok[pi]='\0';
        for(int k=(int)strlen(st_tok)-1;k>=0 && (st_tok[k]=='\r' || st_tok[k]==' '||st_tok[k]=='\t');k--) st_tok[k]='\0';
        for(int k=(int)strlen(dir_tok)-1;k>=0 && (dir_tok[k]=='\r' || dir_tok[k]==' '||dir_tok[k]=='\t');k--) dir_tok[k]='\0';
        for(int k=(int)strlen(path_tok)-1;k>=0 && (path_tok[k]=='\r' || path_tok[k]==' '||path_tok[k]=='\t');k--) path_tok[k]='\0';
        int sidx = state_name_to_index(st_tok);
        int didx = dir_name_to_index(dir_tok);
        if(sidx<0 || didx<0) continue;
#if defined(_MSC_VER)
        strncpy_s(g_app.player_sheet_path[sidx][didx], sizeof g_app.player_sheet_path[sidx][didx], path_tok, _TRUNCATE);
#else
        strncpy(g_app.player_sheet_path[sidx][didx], path_tok, sizeof g_app.player_sheet_path[sidx][didx]-1);
        g_app.player_sheet_path[sidx][didx][sizeof g_app.player_sheet_path[sidx][didx]-1]='\0';
#endif
        if(strcmp(dir_tok,"side")==0){
#if defined(_MSC_VER)
            strncpy_s(g_app.player_sheet_path[sidx][2], sizeof g_app.player_sheet_path[sidx][2], path_tok, _TRUNCATE);
#else
            strncpy(g_app.player_sheet_path[sidx][2], path_tok, sizeof g_app.player_sheet_path[sidx][2]-1);
            g_app.player_sheet_path[sidx][2][sizeof g_app.player_sheet_path[sidx][2]-1]='\0';
#endif
    }
    loaded++;
    }
    fclose(f);
    if(loaded>0){ g_app.player_sheet_paths_loaded = 1; ROGUE_LOG_INFO("player sheet config loaded %d entries", loaded); }
}

static void load_player_anim_config(const char* path){
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if(!f) return;
    char line[512];
    while(fgets(line,sizeof line,f)){
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(*p=='#' || *p=='\n' || *p=='\0') continue;
        char act[32], dir[32]; char* cursor = p;
        int ai=0; while(*cursor && *cursor!=',' && ai<31){ act[ai++]=*cursor++; } act[ai]='\0'; if(*cursor!=',') continue; cursor++;
        int di=0; while(*cursor && *cursor!=',' && di<31){ dir[di++]=*cursor++; } dir[di]='\0'; if(*cursor!=',') continue; cursor++;
        int times[16]; int tcount=0;
        while(*cursor && tcount<16){ while(*cursor==' '||*cursor=='\t') cursor++; if(*cursor=='\n' || *cursor=='\0') break; int val=atoi(cursor); if(val<=0) val=120; times[tcount++]=val; while(*cursor && *cursor!=',' && *cursor!='\n') cursor++; if(*cursor==',') cursor++; }
        int s=-1; if(strcmp(act,"idle")==0) s=0; else if(strcmp(act,"walk")==0) s=1; else if(strcmp(act,"run")==0) s=2; else if(strcmp(act,"attack")==0) s=3; else continue;
        int d=-1; if(strcmp(dir,"down")==0) d=0; else if(strcmp(dir,"side")==0) d=1; else if(strcmp(dir,"up")==0) d=3; else continue;
        if(tcount==0) continue;
        for(int i=0;i<tcount && i<8;i++) g_app.player_frame_time_ms[s][d][i] = times[i];
    }
    fclose(f);
}

void rogue_player_assets_ensure_loaded(void){
    if(g_app.player_loaded) return;
    const char* state_names[4]  = {"idle","walk","run","attack"};
    const char* dir_names[4]    = {"down","side","right","up"};
    if(!g_app.player_sheet_paths_loaded){
        load_player_sheet_paths("../assets/player_sheets.cfg");
        for(int s=0;s<4;s++) for(int d=0; d<4; d++) if(!g_app.player_sheet_path[s][d][0]){
            char defp[256]; const char* use_dir = (d==1||d==2)? "side" : dir_names[d];
            snprintf(defp,sizeof defp,"assets/character/%s_%s.png", state_names[s], use_dir);
#if defined(_MSC_VER)
            strncpy_s(g_app.player_sheet_path[s][d], sizeof g_app.player_sheet_path[s][d], defp, _TRUNCATE);
#else
            strncpy(g_app.player_sheet_path[s][d], defp, sizeof g_app.player_sheet_path[s][d]-1); g_app.player_sheet_path[s][d][sizeof g_app.player_sheet_path[s][d]-1]='\0';
#endif
        }
    }
    int any_player_texture_loaded = 0;
    for(int s=0;s<4;s++) for(int d=0; d<4; d++){
        const char* path = g_app.player_sheet_path[s][d];
        if(rogue_texture_load(&g_app.player_tex[s][d], path)){
            any_player_texture_loaded = 1; g_app.player_sheet_loaded[s][d] = 1;
            int texw=g_app.player_tex[s][d].w, texh=g_app.player_tex[s][d].h;
            if(texh>0 && texh != g_app.player_frame_size){ ROGUE_LOG_INFO("Auto-adjust player frame size from %d to %d (sheet: %s)", g_app.player_frame_size, texh, path); g_app.player_frame_size = texh; }
            int frames = (g_app.player_frame_size>0)? (texw / g_app.player_frame_size) : 0; if(frames>8) frames=8; if(frames<=0){ ROGUE_LOG_WARN("Player sheet width %d < frame_size %d; forcing single frame: %s", texw, g_app.player_frame_size, path); frames=1; }
            g_app.player_frame_count[s][d] = frames;
            for(int f=0; f<frames; f++){
                g_app.player_frames[s][d][f].tex = &g_app.player_tex[s][d];
                g_app.player_frames[s][d][f].sx = f * g_app.player_frame_size;
                g_app.player_frames[s][d][f].sy = 0;
                int remaining = texw - f*g_app.player_frame_size; if(remaining < g_app.player_frame_size) remaining = (remaining>0)? remaining : g_app.player_frame_size;
                g_app.player_frames[s][d][f].sw = (remaining > g_app.player_frame_size)? g_app.player_frame_size : remaining;
                g_app.player_frames[s][d][f].sh = (texh < g_app.player_frame_size)? texh : g_app.player_frame_size;
                int base = (s==0)? 160 : (s==2? 90 : 120);
                g_app.player_frame_time_ms[s][d][f] = base;
            }
            for(int f=g_app.player_frame_count[s][d]; f<8; f++){ g_app.player_frames[s][d][f].tex=&g_app.player_tex[s][d]; g_app.player_frames[s][d][f].sw=0; }
        } else { ROGUE_LOG_WARN("Failed to load player sheet: %s (state=%d dir=%d)", path,s,d); g_app.player_sheet_loaded[s][d]=0; }
    }
    g_app.player_loaded = any_player_texture_loaded; if(!g_app.player_loaded) ROGUE_LOG_WARN("No player sprite sheets loaded; using placeholder rectangle.");
    load_player_anim_config("assets/player_anim.cfg");
}

void rogue_player_assets_update_animation(float frame_dt_ms, float dt_ms, float raw_dt_ms, int attack_pressed){
    (void)raw_dt_ms; (void)frame_dt_ms; /* frame_dt_ms currently unused after refactor */
    static int prev_attack_phase = -1;
    rogue_combat_update_player(&g_app.player_combat, dt_ms, attack_pressed);
    if(g_app.player_combat.phase != prev_attack_phase){
        if(g_app.player_combat.phase == ROGUE_ATTACK_WINDUP){ g_app.player.anim_frame=0; g_app.player.anim_time=0.0f; g_app.attack_anim_time_ms=0.0f; }
        else if(g_app.player_combat.phase == ROGUE_ATTACK_IDLE && prev_attack_phase != -1){ g_app.player.anim_frame=0; g_app.player.anim_time=0.0f; }
        prev_attack_phase = g_app.player_combat.phase;
    }
    int kills = rogue_combat_player_strike(&g_app.player_combat, &g_app.player, g_app.enemies, g_app.enemy_count);
    if(kills>0){ g_app.total_kills += kills; g_app.player.xp += kills * (3 + g_app.player.level); }
}
