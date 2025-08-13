/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "core/app.h"
#include "core/app_state.h"
#include "core/game_loop.h"
#include "util/log.h"
#include "graphics/font.h"
#include "world/world_gen.h"
#include "input/input.h"
#include "entities/player.h"
#include "entities/enemy.h"
#include "game/combat.h"
#include "game/damage_numbers.h"
#include "core/enemy_system.h"
#include "core/start_screen.h"
#include "core/player_assets.h"
#include "core/player_controller.h"
#include "core/minimap.h"
#include "graphics/sprite.h"
#include "graphics/tile_sprites.h"
#include "core/input_events.h"
#include "core/player_render.h"
#include "core/enemy_render.h"
#include "core/hud.h"
#include "core/player_progress.h"
#include "game/damage_numbers.h" /* will provide render/update */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <time.h> /* for local timing when SDL not providing high-res */

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/* state now in app_state.c */

int rogue_get_current_attack_frame(void){
    /* Return current player anim frame if attack state active, else 0 */
    if(g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER){
        return g_app.player.anim_frame;
    }
    return 0;
}


/* tile collision helper moved to player_controller */

int rogue_app_player_health(void){ return g_app.player.health; }

RogueEnemy* rogue_test_spawn_hostile_enemy(float x, float y){
    if(g_app.enemy_type_count<=0) return NULL;
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++) if(!g_app.enemies[i].alive){
        RogueEnemy* ne = &g_app.enemies[i];
        /* Spawn relative to player position so tests can provide small offsets independent of world gen spawn location */
        float px = g_app.player.base.pos.x + x;
        float py = g_app.player.base.pos.y + y;
        ne->base.pos.x = px; ne->base.pos.y = py; ne->anchor_x = px; ne->anchor_y = py;
    ne->patrol_target_x = px; ne->patrol_target_y = py; /* keep stationary for deterministic DPS timing */
        ne->max_health = 10; ne->health = 10; ne->alive=1; ne->hurt_timer=0; ne->anim_time=0; ne->anim_frame=0;
    ne->ai_state = ROGUE_ENEMY_AI_AGGRO; ne->facing=2; ne->type_index=0; ne->tint_r=255; ne->tint_g=255; ne->tint_b=255; ne->death_fade=1.0f; ne->tint_phase=0; ne->flash_timer=0; ne->attack_cooldown_ms=0; /* immediate first attack allowed */
    ne->crit_chance = 5; ne->crit_damage = 25;
        g_app.enemy_count++; g_app.per_type_counts[0]++;
        return ne;
    }
    return NULL;
}

/* damage number functions moved to damage_numbers.c */

/* Public helper to add hitstop (clamped) */
void rogue_app_add_hitstop(float ms){
    if(ms < 0) return;
    if(ms > 180.0f) ms = 180.0f;
    if(g_app.hitstop_timer_ms < ms) g_app.hitstop_timer_ms = ms; /* take longer value */
}

/* Local timing helper (separate from game_loop's static) */
static double now_seconds(void) { return (double) clock() / (double) CLOCKS_PER_SEC; }

/* Load player animation frame durations from config file. */
static void load_player_anim_config(const char* path){
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if(!f) return; /* silent fallback */
    char line[512];
    while(fgets(line,sizeof line,f)){
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(*p=='#' || *p=='\n' || *p=='\0') continue;
        char act[32], dir[32];
        char* cursor = p;
        /* parse activity */
        int ai=0; while(*cursor && *cursor!=',' && ai<31){ act[ai++]=*cursor++; } act[ai]='\0'; if(*cursor!=',') continue; cursor++;
        int di=0; while(*cursor && *cursor!=',' && di<31){ dir[di++]=*cursor++; } dir[di]='\0'; if(*cursor!=',') continue; cursor++;
        /* remaining are frame times separated by commas or newline */
        int times[16]; int tcount=0;
        while(*cursor && tcount<16){
            while(*cursor==' '||*cursor=='\t') cursor++;
            if(*cursor=='\n' || *cursor=='\0') break;
            int val = atoi(cursor); if(val<=0) val=120; times[tcount++]=val;
            while(*cursor && *cursor!=',' && *cursor!='\n') cursor++;
            if(*cursor==',') cursor++;
        }
        int s=-1; if(strcmp(act,"idle")==0) s=0; else if(strcmp(act,"walk")==0) s=1; else if(strcmp(act,"run")==0) s=2; else if(strcmp(act,"attack")==0) s=3; else continue;
        int d=-1; if(strcmp(dir,"down")==0) d=0; else if(strcmp(dir,"side")==0) d=1; else if(strcmp(dir,"up")==0) d=3; else continue;
        if(tcount==0) continue;
        /* apply times (will clamp to frame count later) */
        for(int i=0;i<tcount && i<8;i++) g_app.player_frame_time_ms[s][d][i] = times[i];
    }
    fclose(f);
}

/* Load sound effect paths (simple: LEVELUP,<path>) */
static void load_sound_config(const char* path){
#ifdef ROGUE_HAVE_SDL_MIXER
    FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return;
    char line[512];
    while(fgets(line,sizeof line,f)){
        char* p=line; while(*p==' '||*p=='\t') p++;
        if(*p=='#' || *p=='\n' || *p=='\0') continue;
        if(strncmp(p,"LEVELUP",7)==0){ p+=7; if(*p==',') p++; while(*p==' '||*p=='\t') p++; char path_tok[400]; int i=0; while(p[i] && p[i]!='\n' && i<399){ path_tok[i]=p[i]; i++; } path_tok[i]='\0';
            if(g_app.sfx_levelup) { Mix_FreeChunk(g_app.sfx_levelup); g_app.sfx_levelup=NULL; }
            g_app.sfx_levelup = Mix_LoadWAV(path_tok);
            if(!g_app.sfx_levelup){ ROGUE_LOG_WARN("Failed to load levelup sound: %s", path_tok); }
            else { ROGUE_LOG_INFO("Loaded levelup sound: %s", path_tok); }
        }
    }
    fclose(f);
#else
    (void)path;
#endif
}

static void apply_window_mode(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.window) return;
    Uint32 flags = 0;
    switch (g_app.cfg.window_mode)
    {
    case ROGUE_WINDOW_FULLSCREEN: flags = SDL_WINDOW_FULLSCREEN; break;
    case ROGUE_WINDOW_BORDERLESS: flags = SDL_WINDOW_FULLSCREEN_DESKTOP; break;
    case ROGUE_WINDOW_WINDOWED: default: flags = 0; break; }
    if (SDL_SetWindowFullscreen(g_app.window, flags) != 0)
    {
        ROGUE_LOG_WARN("Failed to set fullscreen mode: %s", SDL_GetError());
    }
#endif
}

bool rogue_app_init(const RogueAppConfig* cfg)
{
    g_app.cfg = *cfg;
    g_app.show_start_screen = 1;
    rogue_input_clear(&g_app.input);
    g_app.title_time = 0.0;
    g_app.menu_index = 0;
    g_app.entering_seed = 0;
    g_app.pending_seed = 1337u;
#ifdef ROGUE_HAVE_SDL
    Uint32 sdl_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#ifdef ROGUE_HAVE_SDL_MIXER
    sdl_flags |= SDL_INIT_AUDIO;
#endif
    if (SDL_Init(sdl_flags) != 0)
    {
        ROGUE_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    Uint32 win_flags = SDL_WINDOW_SHOWN;
    if (cfg->allow_resize) win_flags |= SDL_WINDOW_RESIZABLE;
    g_app.window = SDL_CreateWindow(cfg->window_title, SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, cfg->window_width, cfg->window_height, win_flags);
    if (!g_app.window)
    {
        ROGUE_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    Uint32 rflags = SDL_RENDERER_ACCELERATED;
    if (cfg->enable_vsync) rflags |= SDL_RENDERER_PRESENTVSYNC;
    g_app.renderer = SDL_CreateRenderer(g_app.window, -1, rflags);
    if (!g_app.renderer)
    {
        ROGUE_LOG_WARN("SDL_CreateRenderer failed (%s). Entering headless test mode (no rendering).", SDL_GetError());
        g_app.headless = 1;
    }
    /* Temporary exposure of SDL_Renderer for renderer abstraction */
    extern SDL_Renderer* g_internal_sdl_renderer_ref;
    g_internal_sdl_renderer_ref = g_app.renderer;
#else
    (void) cfg;
#endif
    /* Configure logical size & scaling */
    if (cfg->logical_width > 0 && cfg->logical_height > 0)
    {
#ifdef ROGUE_HAVE_SDL
        SDL_RenderSetLogicalSize(g_app.renderer, cfg->logical_width, cfg->logical_height);
        if (cfg->integer_scale) SDL_RenderSetIntegerScale(g_app.renderer, SDL_TRUE);
#endif
    }

    apply_window_mode();

    RogueGameLoopConfig loop_cfg = {.target_fps = cfg->target_fps};
    rogue_game_loop_init(&loop_cfg);

    /* Load persisted generation params if available BEFORE first world gen */
    g_app.gen_water_level = 0.34; g_app.gen_noise_octaves = 6; g_app.gen_noise_gain = 0.48; g_app.gen_noise_lacunarity = 2.05; g_app.gen_river_sources = 10; g_app.gen_river_max_length = 1200; g_app.gen_cave_thresh = 0.60; g_app.gen_params_dirty = 0;
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, "gen_params.cfg", "rb");
#else
        f = fopen("gen_params.cfg", "rb");
#endif
        if(f){
            char line[256];
            while(fgets(line,sizeof line,f)){
                if(line[0]=='#' || line[0]=='\n' || line[0]=='\0') continue;
                char* eq = strchr(line,'='); if(!eq) continue; *eq='\0';
                char* key=line; char* val=eq+1;
                /* trim */
                for(char* p=key; *p; ++p){ if(*p=='\r' || *p=='\n') { *p='\0'; break; } }
                for(char* p=val; *p; ++p){ if(*p=='\r' || *p=='\n') { *p='\0'; break; } }
                if(strcmp(key,"WATER_LEVEL")==0) g_app.gen_water_level = atof(val);
                else if(strcmp(key,"NOISE_OCTAVES")==0) g_app.gen_noise_octaves = atoi(val);
                else if(strcmp(key,"NOISE_GAIN")==0) g_app.gen_noise_gain = atof(val);
                else if(strcmp(key,"NOISE_LACUNARITY")==0) g_app.gen_noise_lacunarity = atof(val);
                else if(strcmp(key,"RIVER_SOURCES")==0) g_app.gen_river_sources = atoi(val);
                else if(strcmp(key,"RIVER_MAX_LENGTH")==0) g_app.gen_river_max_length = atoi(val);
                else if(strcmp(key,"CAVE_THRESH")==0) g_app.gen_cave_thresh = atof(val);
            }
            fclose(f);
        }
    }
    /* Pre-generate world using (possibly loaded) params */
    RogueWorldGenConfig wcfg = { .seed = 1337u, .width = 80, .height = 60, .biome_regions = 10, .cave_iterations = 3, .cave_fill_chance = 0.45, .river_attempts = 2, .small_island_max_size = 3, .small_island_passes = 2, .shore_fill_passes = 1, .advanced_terrain = 1, .water_level = g_app.gen_water_level, .noise_octaves = g_app.gen_noise_octaves, .noise_gain = g_app.gen_noise_gain, .noise_lacunarity = g_app.gen_noise_lacunarity, .river_sources = g_app.gen_river_sources, .river_max_length = g_app.gen_river_max_length, .cave_mountain_elev_thresh = g_app.gen_cave_thresh };
    /* Scale world 10x while keeping similar structural density by scaling biome_regions proportionally to area. */
    wcfg.width *= 10; /* 800 */
    wcfg.height *= 10; /* 600 */
    /* Maintain roughly same biome seed density: original had 10 seeds over 4800 tiles (~1 per 480). New area 480000 so scale seeds ~ area ratio */
    wcfg.biome_regions = 10 * 100; /* 1000 seeds */
    rogue_world_generate(&g_app.world_map, &wcfg);
    rogue_player_init(&g_app.player);
    g_exposed_player_for_stats = g_app.player;
    g_app.unspent_stat_points = 0;
    g_app.stats_dirty = 0;
    g_app.show_stats_panel = 0;
    g_app.stats_panel_index = 0;
    g_app.time_since_player_hit_ms = 0.0f;
    g_app.health_regen_accum_ms = 0.0f;
    g_app.mana_regen_accum_ms = 0.0f;
    g_app.levelup_aura_timer_ms = 0.0f;
    g_app.dmg_number_count = 0; g_app.spawn_accum_ms = 700.0; /* start beyond threshold so first spawn attempt happens next frame */
#ifdef ROGUE_HAVE_SDL_MIXER
    g_app.sfx_levelup = NULL;
    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512)!=0){
        ROGUE_LOG_WARN("Mix_OpenAudio failed: %s", Mix_GetError());
    } else {
        load_sound_config("assets/sounds.cfg");
    }
#endif
    /* Load persisted player stats */
    {
        FILE* f=NULL; 
#if defined(_MSC_VER)
        fopen_s(&f, "player_stats.cfg", "rb");
#else
        f=fopen("player_stats.cfg","rb");
#endif
        if(f){
            char line[256];
            while(fgets(line,sizeof line,f)){
                if(line[0]=='#' || line[0]=='\n' || line[0]=='\0') continue;
                char* eq=strchr(line,'='); if(!eq) continue; *eq='\0';
                char* key=line; char* val=eq+1;
                for(char* p=key; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
                for(char* p=val; *p; ++p){ if(*p=='\r'||*p=='\n'){*p='\0';break;} }
                if(strcmp(key,"LEVEL")==0) g_app.player.level = atoi(val);
                else if(strcmp(key,"XP")==0) g_app.player.xp = atoi(val);
                else if(strcmp(key,"XP_TO_NEXT")==0) g_app.player.xp_to_next = atoi(val);
                else if(strcmp(key,"STR")==0) g_app.player.strength = atoi(val);
                else if(strcmp(key,"DEX")==0) g_app.player.dexterity = atoi(val);
                else if(strcmp(key,"VIT")==0) g_app.player.vitality = atoi(val);
                else if(strcmp(key,"INT")==0) g_app.player.intelligence = atoi(val);
                else if(strcmp(key,"CRITC")==0) g_app.player.crit_chance = atoi(val);
                else if(strcmp(key,"CRITD")==0) g_app.player.crit_damage = atoi(val);
                else if(strcmp(key,"UNSPENT")==0) g_app.unspent_stat_points = atoi(val);
                else if(strcmp(key,"HP")==0) g_app.player.health = atoi(val);
                else if(strcmp(key,"MP")==0) g_app.player.mana = atoi(val);
            }
            fclose(f);
            rogue_player_recalc_derived(&g_app.player);
            g_app.stats_dirty = 0; /* loaded */
        }
    }
    g_app.tileset_loaded = 0; /* registry not finalized yet */
    g_app.tile_size = 16; /* terrain tiles */
    g_app.player_frame_size = 64;
    g_app.player_loaded = 0;
    g_app.player_state = 0;
    g_app.player_sheet_paths_loaded = 0;
    g_app.cam_x = 0.0f; g_app.cam_y = 0.0f;
    g_app.viewport_w = cfg->logical_width ? cfg->logical_width : cfg->window_width;
    g_app.viewport_h = cfg->logical_height ? cfg->logical_height : cfg->window_height;
    g_app.walk_speed = 45.0f;  /* increased from 3.0 */
    g_app.run_speed  = 85.0f;  /* increased from 6.0 */
    g_app.tile_sprite_lut = NULL;
    g_app.tile_sprite_lut_ready = 0;
    g_app.minimap_dirty = 1;
    g_app.minimap_w = g_app.minimap_h = 0; g_app.minimap_step = 1;
#ifdef ROGUE_HAVE_SDL
    g_app.minimap_tex = NULL;
#endif
    g_app.chunk_size = 32; g_app.chunks_x = g_app.chunks_y = 0; g_app.chunk_dirty = NULL;
    g_app.anim_dt_accum_ms = 0.0f;
    g_app.frame_draw_calls = 0;
    g_app.frame_tile_quads = 0;
    /* Values already loaded (or defaults applied) above */
    /* Init combat */
    rogue_combat_init(&g_app.player_combat);
    g_app.enemy_count = 0; g_app.total_kills = 0;
    g_app.enemy_type_count = ROGUE_MAX_ENEMY_TYPES; /* capacity passed to loader */
    if(!rogue_enemy_load_config("assets/enemies.cfg", g_app.enemy_types, &g_app.enemy_type_count) || g_app.enemy_type_count<=0){
    ROGUE_LOG_WARN("No enemy types loaded; injecting fallback dummy type.");
    g_app.enemy_type_count = 1;
    RogueEnemyTypeDef* t = &g_app.enemy_types[0]; memset(t,0,sizeof *t);
#if defined(_MSC_VER)
    strncpy_s(t->name,sizeof t->name,"dummy",_TRUNCATE);
#else
    strncpy(t->name,"dummy",sizeof t->name -1); t->name[sizeof t->name -1]='\0';
#endif
    t->group_min=1; t->group_max=2; t->patrol_radius=5; t->aggro_radius=6; t->speed=30.0f; t->pop_target=15; t->xp_reward=2; t->loot_chance=0.05f;
    } else {
    ROGUE_LOG_INFO("Loaded %d enemy types", g_app.enemy_type_count);
    }
    /* Ensure per-type population counters start at zero (uninitialized memory previously blocked spawns) */
    for(int i=0;i<ROGUE_MAX_ENEMY_TYPES;i++){ g_app.per_type_counts[i]=0; }
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++){ g_app.enemies[i].alive=0; }
    /* Log current working directory for asset path debugging */
    {
        char cwd_buf[512];
#ifdef _WIN32
        if(_getcwd(cwd_buf, sizeof cwd_buf)){
            ROGUE_LOG_INFO("CWD: %s", cwd_buf);
        } else { ROGUE_LOG_WARN("Could not determine CWD"); }
#else
        if(getcwd(cwd_buf, sizeof cwd_buf)){
            ROGUE_LOG_INFO("CWD: %s", cwd_buf);
        } else { ROGUE_LOG_WARN("Could not determine CWD"); }
#endif
    }
    return true;
}

/* input event loop moved to input_events.c */

/* minimap helpers moved to minimap.c */

/* Map tokens to indices */
static int state_name_to_index(const char* s){ if(strcmp(s,"idle")==0) return 0; if(strcmp(s,"walk")==0) return 1; if(strcmp(s,"run")==0) return 2; if(strcmp(s,"attack")==0) return 3; return -1; }
static int dir_name_to_index(const char* d){
    if(strcmp(d,"down")==0) return 0;
    if(strcmp(d,"left")==0) return 1; /* treat left/right separately if provided */
    if(strcmp(d,"right")==0) return 2;
    if(strcmp(d,"side")==0) return 1; /* side covers left+right */
    if(strcmp(d,"up")==0) return 3;
    return -1;
}

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
    /* Trim trailing whitespace / carriage returns */
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
        /* If direction was 'side', duplicate for both left/right */
        if(strcmp(dir_tok,"side")==0){
#if defined(_MSC_VER)
            strncpy_s(g_app.player_sheet_path[sidx][2], sizeof g_app.player_sheet_path[sidx][2], path_tok, _TRUNCATE);
#else
            strncpy(g_app.player_sheet_path[sidx][2], path_tok, sizeof g_app.player_sheet_path[sidx][2]-1);
            g_app.player_sheet_path[sidx][2][sizeof g_app.player_sheet_path[sidx][2]-1]='\0';
#endif
        }
        /* Existence pre-check (helps early diagnosis) */
        {
            const char* prefixes[] = {"","../","../../","../../../"};
            char attempt[512]; FILE* tf=NULL; int found=0;
            for(size_t ip=0; ip<sizeof(prefixes)/sizeof(prefixes[0]); ip++){
#if defined(_MSC_VER)
                snprintf(attempt,sizeof attempt,"%s%s", prefixes[ip], g_app.player_sheet_path[sidx][didx]);
                fopen_s(&tf, attempt, "rb");
#else
                snprintf(attempt,sizeof attempt,"%s%s", prefixes[ip], g_app.player_sheet_path[sidx][didx]);
                tf=fopen(attempt,"rb");
#endif
                if(tf){ fclose(tf); if(ip>0) ROGUE_LOG_INFO("player sheet pre-check found via %s", attempt); found=1; break; }
            }
            if(!found){ ROGUE_LOG_WARN("player sheet pre-check NOT found: %s", g_app.player_sheet_path[sidx][didx]); }
        }
        loaded++;
    }
    fclose(f);
    if(loaded>0){ g_app.player_sheet_paths_loaded = 1; ROGUE_LOG_INFO("player sheet config loaded %d entries", loaded); }
}

void rogue_app_step(void)
{
    if (!g_game_loop.running)
        return;
    rogue_process_events();
    double frame_start = now_seconds();
#ifdef ROGUE_HAVE_SDL
    g_app.title_time += g_app.dt;
    SDL_SetRenderDrawColor(g_app.renderer, g_app.cfg.background_color.r, g_app.cfg.background_color.g,
                           g_app.cfg.background_color.b, g_app.cfg.background_color.a);
    SDL_RenderClear(g_app.renderer);
    if (rogue_start_screen_active())
    {
        rogue_start_screen_update_and_render();
    }
    else
    {
    g_app.frame_draw_calls = 0; g_app.frame_tile_quads = 0; /* reset metrics each frame */
    /* Lazy one-time load of assets (avoid repeated loads). Adjust paths to actual asset locations. */
        if(!g_app.tileset_loaded)
        {
            rogue_tile_sprites_init(g_app.tile_size);
            /* Attempt to load external config; fallback to defaults if not present */
            if(!rogue_tile_sprites_load_config("assets/tiles.cfg")){
                rogue_tile_sprite_define(ROGUE_TILE_GRASS, "assets/tiles.png", 0, 0);
                rogue_tile_sprite_define(ROGUE_TILE_WATER, "assets/tiles.png", 1, 0);
                rogue_tile_sprite_define(ROGUE_TILE_FOREST, "assets/tiles.png", 2, 0);
                rogue_tile_sprite_define(ROGUE_TILE_MOUNTAIN, "assets/tiles.png", 3, 0);
                rogue_tile_sprite_define(ROGUE_TILE_CAVE_WALL, "assets/tiles.png", 4, 0);
                rogue_tile_sprite_define(ROGUE_TILE_CAVE_FLOOR, "assets/tiles.png", 5, 0);
                rogue_tile_sprite_define(ROGUE_TILE_RIVER, "assets/tiles.png", 6, 0);
            }
            /* Only mark tileset loaded if finalize succeeded (at least one variant loaded). */
            g_app.tileset_loaded = rogue_tile_sprites_finalize();
            if(g_app.tileset_loaded){
                size_t total = (size_t)g_app.world_map.width * (size_t)g_app.world_map.height;
                g_app.tile_sprite_lut = (const RogueSprite**)malloc(total * sizeof(const RogueSprite*));
            if(g_app.tileset_loaded){
                    for(int y=0; y<g_app.world_map.height; y++){
                        for(int x=0; x<g_app.world_map.width; x++){
                            unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                            const RogueSprite* spr = NULL;
                            if(t < ROGUE_TILE_MAX){
                                spr = rogue_tile_sprite_get_xy((RogueTileType)t, x, y);
                            }
                            g_app.tile_sprite_lut[y*g_app.world_map.width + x] = spr;
                        }
                    }
                    g_app.tile_sprite_lut_ready = 1;
                    ROGUE_LOG_INFO("Precomputed tile sprite LUT built (%dx%d)", g_app.world_map.width, g_app.world_map.height);
                } else {
                    ROGUE_LOG_WARN("Failed to allocate tile sprite LUT (%zu entries)", total);
                }
            } else {
                ROGUE_LOG_WARN("Tile sprites finalize failed; falling back to debug colored tiles.");
            }
        }
    if(!g_app.player_loaded) { rogue_player_assets_ensure_loaded(); }

    /* Update player position / camera */
    rogue_player_controller_update();
    /* Attack input (SPACE/RETURN maps to ACTION) */
    int attack_pressed = rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION);
    float raw_dt_ms = (float)g_app.dt * 1000.0f;
    if(g_app.hitstop_timer_ms > 0){
        g_app.hitstop_timer_ms -= raw_dt_ms;
        if(g_app.hitstop_timer_ms < 0) g_app.hitstop_timer_ms = 0;
    }
    float hitstop_scale = (g_app.hitstop_timer_ms > 0)? 0.25f : 1.0f; /* quarter speed during hitstop */
    float dt_ms = raw_dt_ms * hitstop_scale;
    rogue_player_assets_update_animation(raw_dt_ms, dt_ms, raw_dt_ms, attack_pressed);
    /* Progression (leveling, regen, autosave) */
    rogue_player_progress_update(g_app.dt);
        /* Wrap inside map bounds */
        if(g_app.player.base.pos.x < 0) g_app.player.base.pos.x = 0;
        if(g_app.player.base.pos.y < 0) g_app.player.base.pos.y = 0;
        if(g_app.player.base.pos.x > g_app.world_map.width-1) g_app.player.base.pos.x = (float)(g_app.world_map.width-1);
        if(g_app.player.base.pos.y > g_app.world_map.height-1) g_app.player.base.pos.y = (float)(g_app.world_map.height-1);
    /* Enemy spawning & AI updates */
    rogue_enemy_system_update(dt_ms);

        /* Advance animation (coalesce tiny dt <1ms to reduce float churn) */
        float frame_dt_ms = (float)g_app.dt * 1000.0f;
        if(frame_dt_ms < 1.0f){
            g_app.anim_dt_accum_ms += frame_dt_ms;
            if(g_app.anim_dt_accum_ms >= 1.0f){
                g_app.player.anim_time += g_app.anim_dt_accum_ms;
                g_app.anim_dt_accum_ms = 0.0f;
            }
        } else {
            g_app.player.anim_time += frame_dt_ms;
        }
    int anim_dir = g_app.player.facing;
    int anim_sheet_dir = (anim_dir==1 || anim_dir==2)? 1 : anim_dir;
    int state_for_anim = (g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER)? 3 : g_app.player_state;
    int frame_count = g_app.player_frame_count[state_for_anim][anim_sheet_dir];
        if(frame_count <=0) frame_count = 1;
        if(state_for_anim == 3){
            /* Custom timeline-driven attack animation spanning all phases */
            const float WINDUP_MS = 120.0f;
            const float STRIKE_MS = 80.0f;
            const float RECOVER_MS = 140.0f;
            float total = WINDUP_MS + STRIKE_MS + RECOVER_MS; /* 340ms */
            if(g_app.player_combat.phase==ROGUE_ATTACK_WINDUP || g_app.player_combat.phase==ROGUE_ATTACK_STRIKE || g_app.player_combat.phase==ROGUE_ATTACK_RECOVER){
                g_app.attack_anim_time_ms += frame_dt_ms; if(g_app.attack_anim_time_ms > total) g_app.attack_anim_time_ms = total - 0.01f;
                float t = g_app.attack_anim_time_ms / total; if(t<0) t=0; if(t>1) t=1;
                int idx = (int)(t * frame_count);
                if(idx >= frame_count) idx = frame_count-1;
                g_app.player.anim_frame = idx;
            }
        } else if(state_for_anim == 0){
            g_app.player.anim_frame = 0;
            g_app.player.anim_time = 0.0f;
        } else {
            int cur = g_app.player.anim_frame;
            int cur_dur = g_app.player_frame_time_ms[state_for_anim][anim_sheet_dir][cur];
            if(cur_dur <=0) cur_dur = 120;
            if(g_app.player.anim_time >= (float)cur_dur){
                g_app.player.anim_time = 0.0f;
                g_app.player.anim_frame = (g_app.player.anim_frame + 1) % frame_count;
            }
        }

    /* camera now handled in player_controller */

        /* Render tiles (culled) with horizontal run batching */
        int scale = 1;
        int tsz = g_app.tile_size;
        int first_tx = (int)(g_app.cam_x / tsz); if(first_tx<0) first_tx=0;
        int first_ty = (int)(g_app.cam_y / tsz); if(first_ty<0) first_ty=0;
        int vis_tx = g_app.viewport_w / tsz + 2;
        int vis_ty = g_app.viewport_h / tsz + 2;
        int last_tx = first_tx + vis_tx; if(last_tx > g_app.world_map.width) last_tx = g_app.world_map.width;
        int last_ty = first_ty + vis_ty; if(last_ty > g_app.world_map.height) last_ty = g_app.world_map.height;
        if(g_app.tileset_loaded){
            for(int y=first_ty; y<last_ty; y++){
                int x = first_tx;
                while(x < last_tx){
                    const RogueSprite* spr = NULL;
                    if(g_app.tile_sprite_lut_ready){
                        spr = g_app.tile_sprite_lut[y*g_app.world_map.width + x];
                    } else {
                        unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                        if(t < ROGUE_TILE_MAX) spr = rogue_tile_sprite_get_xy((RogueTileType)t, x, y);
                    }
                    if(!(spr && spr->sw)) { x++; continue; }
                    int run = 1;
                    while(x+run < last_tx){
                        const RogueSprite* spr2 = g_app.tile_sprite_lut_ready ? g_app.tile_sprite_lut[y*g_app.world_map.width + (x+run)] : NULL;
                        if(spr2 != spr) break; /* pointer equality ensures identical source */
                        run++;
                    }
                    /* Draw each tile separately (avoid stretching). Run detection still saves lookups. */
#ifdef ROGUE_HAVE_SDL
                    SDL_Rect src = { spr->sx, spr->sy, spr->sw, spr->sh };
                    for(int i=0;i<run;i++){
                        SDL_Rect dst = { (int)((x+i)*tsz - g_app.cam_x), (int)(y*tsz - g_app.cam_y), tsz*scale, tsz*scale };
                        SDL_RenderCopy(g_app.renderer, spr->tex->handle, &src, &dst);
                    }
#else
                    for(int i=0;i<run;i++) rogue_sprite_draw(spr, (int)((x+i)*tsz - g_app.cam_x), (int)(y*tsz - g_app.cam_y), scale);
#endif
                    x += run;
                }
            }
        } else {
            for(int y=first_ty; y<last_ty; y++){
                for(int x=first_tx; x<last_tx; x++){
                    unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                    RogueColor c = { (unsigned char)(t*20), (unsigned char)(t*15), (unsigned char)(t*10), 255};
                    SDL_SetRenderDrawColor(g_app.renderer, c.r,c.g,c.b,c.a);
                    SDL_Rect r = { (int)(x*tsz - g_app.cam_x), (int)(y*tsz - g_app.cam_y), tsz*scale, tsz*scale };
                    SDL_RenderFillRect(g_app.renderer, &r); g_app.frame_draw_calls++; g_app.frame_tile_quads++;
                }
            }
        }

    /* Render player */
    rogue_player_render();
    /* Render enemies */
    rogue_enemy_render();

    /* Floating damage numbers */
    rogue_damage_numbers_render();

    /* Mini-map in corner (scaled down, render-target cached) */
    /* Minimap */
    rogue_minimap_update_and_render(240);
    }
    /* HUD */
    rogue_hud_render();
    /* Update floating damage numbers */
    rogue_damage_numbers_update((float)g_app.dt);
    /* (Overlay with debug metrics removed to show clean HUD; hotkeys still active) */
    rogue_stats_panel_render();
    if(!g_app.headless){ SDL_RenderPresent(g_app.renderer); }
    /* Refresh exported player after all stat changes this frame */
    g_exposed_player_for_stats = g_app.player;
#endif
    rogue_game_loop_iterate();
    g_app.frame_count++;
    double frame_end = now_seconds();
    g_app.frame_ms = (frame_end - frame_start) * 1000.0;
    g_app.dt = (g_game_loop.target_frame_seconds > 0.0) ? g_game_loop.target_frame_seconds : (frame_end - frame_start);
    /* Headless/unit-test environments can produce extremely tiny dt; enforce a modest minimum (simulate ~120 FPS) */
    if(g_app.dt < 0.0083) g_app.dt = 0.0083;
    if (g_app.dt < 0.0001) g_app.dt = 0.0001; /* clamp */
    g_app.fps = (g_app.dt > 0.0) ? 1.0 / g_app.dt : 0.0;
    g_app.avg_frame_ms_accum += g_app.frame_ms;
    g_app.avg_frame_samples++;
    if (g_app.avg_frame_samples >= 120)
    {
        g_app.avg_frame_ms_accum = g_app.avg_frame_ms_accum / g_app.avg_frame_samples;
        g_app.avg_frame_samples = 0; /* reuse accum as average holder */
    }
    rogue_input_next_frame(&g_app.input);
}

void rogue_app_run(void)
{
    while (g_game_loop.running)
    {
        rogue_app_step();
    }
}

void rogue_app_shutdown(void)
{
#ifdef ROGUE_HAVE_SDL_MIXER
    if(g_app.sfx_levelup){ Mix_FreeChunk(g_app.sfx_levelup); g_app.sfx_levelup=NULL; }
    Mix_CloseAudio();
#endif
#ifdef ROGUE_HAVE_SDL
    if(g_app.minimap_tex){ SDL_DestroyTexture(g_app.minimap_tex); g_app.minimap_tex=NULL; }
    if (g_app.renderer)
        SDL_DestroyRenderer(g_app.renderer);
    if (g_app.window)
        SDL_DestroyWindow(g_app.window);
    SDL_Quit();
#endif
    if(g_app.tile_sprite_lut){ free((void*)g_app.tile_sprite_lut); g_app.tile_sprite_lut=NULL; }
    if(g_app.chunk_dirty){ free(g_app.chunk_dirty); g_app.chunk_dirty=NULL; }
    /* Persist generation params if changed */
    if(g_app.gen_params_dirty){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "gen_params.cfg", "wb");
#else
    f=fopen("gen_params.cfg","wb");
#endif
    if(f){
        fprintf(f,"# Saved world generation parameters\n");
        fprintf(f,"WATER_LEVEL=%.4f\n", g_app.gen_water_level);
        fprintf(f,"NOISE_OCTAVES=%d\n", g_app.gen_noise_octaves);
        fprintf(f,"NOISE_GAIN=%.4f\n", g_app.gen_noise_gain);
        fprintf(f,"NOISE_LACUNARITY=%.4f\n", g_app.gen_noise_lacunarity);
        fprintf(f,"RIVER_SOURCES=%d\n", g_app.gen_river_sources);
        fprintf(f,"RIVER_MAX_LENGTH=%d\n", g_app.gen_river_max_length);
        fprintf(f,"CAVE_THRESH=%.4f\n", g_app.gen_cave_thresh);
        fclose(f);
    }
    }
    /* Persist player stats */
    {
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "player_stats.cfg", "wb");
#else
    f=fopen("player_stats.cfg","wb");
#endif
    if(f){
        fprintf(f,"# Saved player progression\n");
        fprintf(f,"LEVEL=%d\n", g_app.player.level);
        fprintf(f,"XP=%d\n", g_app.player.xp);
        fprintf(f,"XP_TO_NEXT=%d\n", g_app.player.xp_to_next);
        fprintf(f,"STR=%d\n", g_app.player.strength);
        fprintf(f,"DEX=%d\n", g_app.player.dexterity);
        fprintf(f,"VIT=%d\n", g_app.player.vitality);
        fprintf(f,"INT=%d\n", g_app.player.intelligence);
    fprintf(f,"CRITC=%d\n", g_app.player.crit_chance);
    fprintf(f,"CRITD=%d\n", g_app.player.crit_damage);
        fprintf(f,"UNSPENT=%d\n", g_app.unspent_stat_points);
        fprintf(f,"HP=%d\n", g_app.player.health);
    fprintf(f,"MP=%d\n", g_app.player.mana);
        fclose(f);
    }
    }
}

int rogue_app_frame_count(void) { return g_app.frame_count; }
int rogue_app_enemy_count(void) { return g_app.enemy_count; }
void rogue_app_skip_start_screen(void){ g_app.show_start_screen = 0; }

void rogue_app_toggle_fullscreen(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.window) return;
    if (g_app.cfg.window_mode == ROGUE_WINDOW_FULLSCREEN)
        g_app.cfg.window_mode = ROGUE_WINDOW_WINDOWED;
    else
        g_app.cfg.window_mode = ROGUE_WINDOW_FULLSCREEN;
    apply_window_mode();
#endif
}

void rogue_app_set_vsync(int enabled)
{
#ifdef ROGUE_HAVE_SDL
     /* SDL2 does not provide dynamic vsync toggle pre-2.0.18 except by recreating the renderer.
         For simplicity we just store the flag here (no-op for current session). */
     (void) enabled;
     ROGUE_LOG_WARN("rogue_app_set_vsync: dynamic toggle not implemented (requires renderer recreation)");
#else
    (void) enabled;
#endif
}

void rogue_app_get_metrics(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms)
{
    if (out_fps) *out_fps = g_app.fps;
    if (out_frame_ms) *out_frame_ms = g_app.frame_ms;
    if (out_avg_frame_ms)
    {
        double avg = (g_app.avg_frame_samples == 0 && g_app.avg_frame_ms_accum > 0.0)
                          ? g_app.avg_frame_ms_accum
                          : (g_app.avg_frame_ms_accum / (g_app.avg_frame_samples ? g_app.avg_frame_samples : 1));
        *out_avg_frame_ms = avg;
    }
}

double rogue_app_delta_time(void) { return g_app.dt; }
