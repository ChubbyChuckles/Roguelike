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
#include "core/game_loop.h"
#include "util/log.h"
#include "graphics/font.h"
#include "world/world_gen.h"
#include "input/input.h"
#include "entities/player.h"
#include "graphics/sprite.h"
#include "graphics/tile_sprites.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <time.h> /* for local timing when SDL not providing high-res */

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

typedef struct RogueAppState
{
    RogueAppConfig cfg;
#ifdef ROGUE_HAVE_SDL
    SDL_Window* window;
    SDL_Renderer* renderer;
#endif
    int show_start_screen;
    RogueTileMap world_map;
    RogueInputState input;
    RoguePlayer player;
    int tileset_loaded; /* now indicates tile registry finalized */
    int tile_size; /* terrain tile pixel size (now 16) */
    int player_frame_size; /* character frame size (64) */
    /* Player textures for animations: state (0=idle,1=walk,2=run) x direction (0=down,1=left,2=right,3=up) */
    RogueTexture player_tex[3][4];
    RogueSprite  player_frames[3][4][8]; /* up to 8 frames (384 / 64 = 6 actual frames, allocate 8) */
    int          player_frame_count[3][4];
    int          player_frame_time_ms[3][4][8]; /* per-frame timing */
    int player_loaded;
    int player_sheet_loaded[3][4];
    int player_state; /* 0 idle,1 walk,2 run */
    /* Configurable player sheet paths (state x direction) */
    char player_sheet_path[3][4][256];
    int player_sheet_paths_loaded;
    double title_time;
    int menu_index; /* 0=new game,1=quit,2=seed entry */
    int entering_seed;
    unsigned int pending_seed;
    int frame_count;
    double dt;
    double fps;
    double frame_ms;
    double avg_frame_ms_accum;
    int avg_frame_samples;
    /* Camera */
    float cam_x;
    float cam_y;
    int viewport_w;
    int viewport_h;
    /* Movement speeds (tiles per second base units) */
    float walk_speed;
    float run_speed;
    /* Precomputed tile sprite pointers (after tileset finalize) */
    const RogueSprite** tile_sprite_lut; /* size width*height */
    int tile_sprite_lut_ready;
    /* Minimap cache flags */
    int minimap_dirty;
    /* Minimap render target & meta */
#ifdef ROGUE_HAVE_SDL
    SDL_Texture* minimap_tex;
#endif
    int minimap_w;
    int minimap_h;
    int minimap_step;
    /* Chunk system for future dirty updates */
    int chunk_size;
    int chunks_x;
    int chunks_y;
    unsigned char* chunk_dirty; /* size chunks_x*chunks_y */
    /* Animation coalescing accumulator (ms) */
    float anim_dt_accum_ms;
    /* Frame metrics */
    int frame_draw_calls;
    int frame_tile_quads;
    /* Persistent world gen params */
    double gen_water_level;
    int gen_noise_octaves;
    double gen_noise_gain;
    double gen_noise_lacunarity;
    int gen_river_sources;
    int gen_river_max_length;
    double gen_cave_thresh;
    int gen_params_dirty;
} RogueAppState;

static RogueAppState g_app;

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
        int s=-1; if(strcmp(act,"idle")==0) s=0; else if(strcmp(act,"walk")==0) s=1; else if(strcmp(act,"run")==0) s=2; else continue;
        int d=-1; if(strcmp(dir,"down")==0) d=0; else if(strcmp(dir,"side")==0) d=1; else if(strcmp(dir,"up")==0) d=3; else continue;
        if(tcount==0) continue;
        /* apply times (will clamp to frame count later) */
        for(int i=0;i<tcount && i<8;i++) g_app.player_frame_time_ms[s][d][i] = times[i];
    }
    fclose(f);
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
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
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
        ROGUE_LOG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
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

static void process_events(void)
{
#ifdef ROGUE_HAVE_SDL
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_QUIT)
        {
            rogue_game_loop_request_exit();
        }
        rogue_input_process_sdl_event(&g_app.input, &ev);
        if (ev.type == SDL_KEYDOWN && !g_app.show_start_screen)
        {
            if (ev.key.keysym.sym == SDLK_r)
            {
                /* toggle run state manually for demo */
                g_app.player_state = (g_app.player_state == 2) ? 1 : 2;
            }
            /* Debug terrain parameter tweaks */
            if (ev.key.keysym.sym == SDLK_F5) { g_app.gen_water_level -= 0.01; if(g_app.gen_water_level < 0.20) g_app.gen_water_level = 0.20; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F6) { g_app.gen_water_level += 0.01; if(g_app.gen_water_level > 0.55) g_app.gen_water_level = 0.55; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F7) { g_app.gen_noise_octaves++; if(g_app.gen_noise_octaves>9) g_app.gen_noise_octaves=9; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F8) { g_app.gen_noise_octaves--; if(g_app.gen_noise_octaves<3) g_app.gen_noise_octaves=3; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F9) { g_app.gen_river_sources += 2; if(g_app.gen_river_sources>40) g_app.gen_river_sources=40; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F10){ g_app.gen_river_sources -= 2; if(g_app.gen_river_sources<2) g_app.gen_river_sources=2; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F11){ g_app.gen_noise_gain += 0.02; if(g_app.gen_noise_gain>0.8) g_app.gen_noise_gain=0.8; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_F12){ g_app.gen_noise_gain -= 0.02; if(g_app.gen_noise_gain<0.3) g_app.gen_noise_gain=0.3; g_app.gen_params_dirty=1; ev.key.keysym.sym = SDLK_BACKQUOTE; }
            if (ev.key.keysym.sym == SDLK_BACKQUOTE){ /* regen */
                g_app.pending_seed = (unsigned int)SDL_GetTicks();
                RogueWorldGenConfig wcfg = { .seed = g_app.pending_seed, .width = 80, .height = 60, .biome_regions = 10, .cave_iterations = 3, .cave_fill_chance = 0.45, .river_attempts = 2, .small_island_max_size = 3, .small_island_passes = 2, .shore_fill_passes = 1, .advanced_terrain = 1, .water_level = g_app.gen_water_level, .noise_octaves = g_app.gen_noise_octaves, .noise_gain = g_app.gen_noise_gain, .noise_lacunarity = g_app.gen_noise_lacunarity, .river_sources = g_app.gen_river_sources, .river_max_length = g_app.gen_river_max_length, .cave_mountain_elev_thresh = g_app.gen_cave_thresh };
                wcfg.width *=10; wcfg.height *=10; wcfg.biome_regions = 1000; rogue_tilemap_free(&g_app.world_map); rogue_world_generate(&g_app.world_map,&wcfg); g_app.minimap_dirty=1; }
        }
        if (ev.type == SDL_KEYDOWN && g_app.show_start_screen)
        {
            if (g_app.entering_seed)
            {
                if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    /* regenerate */
                    rogue_tilemap_free(&g_app.world_map);
                    RogueWorldGenConfig wcfg = { .seed = g_app.pending_seed, .width = 80, .height = 60, .biome_regions = 10, .cave_iterations = 3, .cave_fill_chance = 0.45, .river_attempts = 2, .small_island_max_size = 3, .small_island_passes = 2, .shore_fill_passes = 1, .advanced_terrain = 1, .water_level = 0.34, .noise_octaves = 6, .noise_gain = 0.48, .noise_lacunarity = 2.05, .river_sources = 10, .river_max_length = 1200, .cave_mountain_elev_thresh = 0.60 };
                    rogue_world_generate(&g_app.world_map, &wcfg);
                    /* Initialize chunk meta (after world gen sets dimensions) */
                    g_app.chunks_x = (g_app.world_map.width  + g_app.chunk_size - 1) / g_app.chunk_size;
                    g_app.chunks_y = (g_app.world_map.height + g_app.chunk_size - 1) / g_app.chunk_size;
                    size_t ctotal = (size_t)g_app.chunks_x * (size_t)g_app.chunks_y;
                    if(ctotal){
                        g_app.chunk_dirty = (unsigned char*)malloc(ctotal);
                        if(g_app.chunk_dirty) memset(g_app.chunk_dirty, 0, ctotal);
                    }
                    g_app.entering_seed = 0;
                }
                else if (ev.key.keysym.sym == SDLK_ESCAPE)
                {
                    g_app.entering_seed = 0;
                }
            }
        }
    }
#endif
}

/* --- Minimap helpers ---------------------------------------------------- */
static void ensure_minimap_target(int w, int h){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.renderer) return;
    if(w<=0||h<=0) return;
    if(g_app.minimap_tex && (w!=g_app.minimap_w || h!=g_app.minimap_h)){
        SDL_DestroyTexture(g_app.minimap_tex); g_app.minimap_tex=NULL;
    }
    if(!g_app.minimap_tex){
        g_app.minimap_tex = SDL_CreateTexture(g_app.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
        if(!g_app.minimap_tex){
            ROGUE_LOG_WARN("minimap texture create %dx%d failed: %s", w,h, SDL_GetError());
            return;
        }
        SDL_SetTextureBlendMode(g_app.minimap_tex, SDL_BLENDMODE_BLEND);
        g_app.minimap_w = w; g_app.minimap_h = h; g_app.minimap_dirty = 1;
    }
#else
    (void)w; (void)h;
#endif
}

static void redraw_minimap_if_needed(int mm_w, int mm_h, int step){
#ifdef ROGUE_HAVE_SDL
    if(!g_app.minimap_tex) return;
    static int frame_counter = 0; frame_counter++;
    int periodic = (frame_counter % 180)==0; /* safety refresh every ~3s */
    if(!g_app.minimap_dirty && !periodic) return;
    SDL_Texture* prev = SDL_GetRenderTarget(g_app.renderer);
    SDL_SetRenderTarget(g_app.renderer, g_app.minimap_tex);
    SDL_SetRenderDrawColor(g_app.renderer,0,0,0,0);
    SDL_RenderClear(g_app.renderer);
    for(int y=0; y<g_app.world_map.height; y+=step){
        for(int x=0; x<g_app.world_map.width; x+=step){
            unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
            RogueColor c = {0,0,0,180};
            switch(t){
                case ROGUE_TILE_WATER: c=(RogueColor){30,90,200,220}; break;
                case ROGUE_TILE_RIVER: c=(RogueColor){50,140,230,220}; break;
                case ROGUE_TILE_RIVER_WIDE: c=(RogueColor){70,170,250,230}; break;
                case ROGUE_TILE_RIVER_DELTA: c=(RogueColor){90,190,250,230}; break;
                case ROGUE_TILE_GRASS: c=(RogueColor){40,160,60,220}; break;
                case ROGUE_TILE_FOREST: c=(RogueColor){10,90,20,220}; break;
                case ROGUE_TILE_SWAMP: c=(RogueColor){50,120,50,220}; break;
                case ROGUE_TILE_MOUNTAIN: c=(RogueColor){120,120,120,220}; break;
                case ROGUE_TILE_SNOW: c=(RogueColor){230,230,240,220}; break;
                case ROGUE_TILE_CAVE_WALL: c=(RogueColor){60,60,60,220}; break;
                case ROGUE_TILE_CAVE_FLOOR: c=(RogueColor){110,80,60,220}; break;
                default: break;
            }
            SDL_SetRenderDrawColor(g_app.renderer, c.r,c.g,c.b,c.a);
            int mx = (int)((float)x / (float)g_app.world_map.width * mm_w);
            int my = (int)((float)y / (float)g_app.world_map.height * mm_h);
            SDL_Rect r = { mx, my, 1, 1 };
            SDL_RenderFillRect(g_app.renderer, &r);
        }
    }
    g_app.minimap_dirty = 0;
    SDL_SetRenderTarget(g_app.renderer, prev);
#else
    (void)mm_w; (void)mm_h; (void)step;
#endif
}

/* Map tokens to indices */
static int state_name_to_index(const char* s){ if(strcmp(s,"idle")==0) return 0; if(strcmp(s,"walk")==0) return 1; if(strcmp(s,"run")==0) return 2; return -1; }
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
    process_events();
    double frame_start = now_seconds();
#ifdef ROGUE_HAVE_SDL
    g_app.title_time += g_app.dt;
    SDL_SetRenderDrawColor(g_app.renderer, g_app.cfg.background_color.r, g_app.cfg.background_color.g,
                           g_app.cfg.background_color.b, g_app.cfg.background_color.a);
    SDL_RenderClear(g_app.renderer);
    if (g_app.show_start_screen)
    {
        RogueColor white = {255,255,255,255};
        int pulse = (int)( (sin(g_app.title_time*2.0)*0.5 + 0.5) * 255.0 );
        RogueColor title_col = { (unsigned char)pulse, (unsigned char)pulse, 255, 255};
        rogue_font_draw_text(40, 60, "ROGUELIKE", 6, title_col);
        const char* menu_items[] = { "New Game", "Quit", "Seed:" };
        int base_y = 140;
        for(int i=0;i<3;i++)
        {
            RogueColor c = (i==g_app.menu_index)?(RogueColor){255,255,0,255}:white;
            rogue_font_draw_text(50, base_y + i*20, menu_items[i], 2, c);
        }
        char seed_line[64];
        snprintf(seed_line, sizeof seed_line, "%u", g_app.pending_seed);
        rogue_font_draw_text(140, base_y + 2*20, seed_line, 2, white);
        if (g_app.entering_seed)
            rogue_font_draw_text(140 + (int)strlen(seed_line)*12, base_y + 2*20, "_", 2, white);
        /* Handle menu navigation */
        if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_DOWN)) g_app.menu_index = (g_app.menu_index+1)%3;
        if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_UP)) g_app.menu_index = (g_app.menu_index+2)%3;
        if (rogue_input_was_pressed(&g_app.input, ROGUE_KEY_ACTION))
        {
            if (g_app.menu_index == 0)
                g_app.show_start_screen = 0; /* start */
            else if (g_app.menu_index == 1)
                rogue_game_loop_request_exit();
            else if (g_app.menu_index == 2)
                g_app.entering_seed = 1;
        }
        if (g_app.entering_seed)
        {
            /* process typed digits */
            for(int i=0;i<g_app.input.text_len;i++)
            {
                char ch = g_app.input.text_buffer[i];
                if(ch >= '0' && ch <= '9')
                {
                    g_app.pending_seed = g_app.pending_seed*10 + (unsigned)(ch - '0');
                }
                else if (ch == 'b' || ch == 'B')
                {
                    g_app.pending_seed /= 10; /* crude backspace mapped to 'b' until full text input */
                }
            }
        }
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
        if(!g_app.player_loaded)
        {
            const char* state_names[3]  = {"idle","walk","run"};
                    g_app.frame_draw_calls++;
                    g_app.frame_tile_quads++;
            const char* dir_names[4]    = {"down","side","right","up"};
            if(!g_app.player_sheet_paths_loaded){
                load_player_sheet_paths("assets/player_sheets.cfg");
                /* Fill defaults for any missing entries */
                for(int s=0;s<3;s++){
                    for(int d=0; d<4; d++){
                        if(!g_app.player_sheet_path[s][d][0]){
                            char defp[256];
                            /* Use side for both left/right by default */
                            const char* use_dir = (d==1||d==2)? "side" : dir_names[d];
                            snprintf(defp,sizeof defp,"assets/character/%s_%s.png", state_names[s], use_dir);
#if defined(_MSC_VER)
                            strncpy_s(g_app.player_sheet_path[s][d], sizeof g_app.player_sheet_path[s][d], defp, _TRUNCATE);
#else
                            strncpy(g_app.player_sheet_path[s][d], defp, sizeof g_app.player_sheet_path[s][d]-1);
                            g_app.player_sheet_path[s][d][sizeof g_app.player_sheet_path[s][d]-1]='\0';
#endif
                        }
                    }
                }
            }
            int any_player_texture_loaded = 0;
            for(int s=0;s<3;s++)
            {
                for(int d=0; d<4; d++)
                {
                    const char* path = g_app.player_sheet_path[s][d];
                    if(rogue_texture_load(&g_app.player_tex[s][d], path))
                    {
                        any_player_texture_loaded = 1;
                        g_app.player_sheet_loaded[s][d] = 1;
                        int texw = g_app.player_tex[s][d].w;
                        int texh = g_app.player_tex[s][d].h;
                        /* Auto-detect frame (cell) size from texture height if different */
                        if(texh > 0 && texh != g_app.player_frame_size){
                            ROGUE_LOG_INFO("Auto-adjust player frame size from %d to %d (sheet: %s)", g_app.player_frame_size, texh, path);
                            g_app.player_frame_size = texh; /* assume square frames */
                        }
                        int frames = (g_app.player_frame_size>0)? (texw / g_app.player_frame_size) : 0;
                        if(frames>8) frames = 8;
                        if(frames <= 0){
                            /* Width smaller than expected frame size; treat whole sheet as single frame */
                            ROGUE_LOG_WARN("Player sheet width %d < frame_size %d; forcing single frame: %s", texw, g_app.player_frame_size, path);
                            frames = 1;
                        }
                        ROGUE_LOG_INFO("Loaded player sheet %s (w=%d h=%d frames=%d state=%d dir=%d)", path, texw, texh, frames, s, d);
                        g_app.player_frame_count[s][d] = frames;
                        for(int f=0; f<frames; f++)
                        {
                            g_app.player_frames[s][d][f].tex = &g_app.player_tex[s][d];
                            g_app.player_frames[s][d][f].sx = f * g_app.player_frame_size;
                            g_app.player_frames[s][d][f].sy = 0;
                            /* Clamp width/height to texture bounds */
                            int remaining = texw - f * g_app.player_frame_size;
                            if(remaining < g_app.player_frame_size) remaining = (remaining>0)? remaining : g_app.player_frame_size;
                            g_app.player_frames[s][d][f].sw = (remaining > g_app.player_frame_size)? g_app.player_frame_size : remaining;
                            g_app.player_frames[s][d][f].sh = (texh < g_app.player_frame_size)? texh : g_app.player_frame_size;
                            /* Assign default per-frame time (idle slower, run faster) */
                            int base = (s==0)? 160 : (s==2? 90 : 120); /* ms */
                            g_app.player_frame_time_ms[s][d][f] = base;
                        }
                        /* Mark remaining frames invalid by zero width */
                        for(int f=frames; f<8; f++)
                        {
                            g_app.player_frames[s][d][f].tex = &g_app.player_tex[s][d];
                            g_app.player_frames[s][d][f].sw = 0;
                        }
                    }
                    else {
                        ROGUE_LOG_WARN("Failed to load player sheet: %s (state=%d dir=%d)", path, s, d);
                        /* Print absolute path attempt for extra clarity */
#ifdef _WIN32
                        char fullp[_MAX_PATH];
                        if(_fullpath(fullp, path, sizeof fullp)){
                            ROGUE_LOG_WARN("Absolute path candidate: %s", fullp);
                        }
#endif
                        g_app.player_sheet_loaded[s][d] = 0;
                    }
                }
            }
            g_app.player_loaded = any_player_texture_loaded;
            if(!g_app.player_loaded){
                ROGUE_LOG_WARN("No player sprite sheets loaded; using placeholder rectangle.");
            }
            load_player_anim_config("assets/player_anim.cfg");
        }

        /* Update player position from input (simple) */
    float speed = (g_app.player_state==2)? g_app.run_speed : (g_app.player_state==1? g_app.walk_speed : 0.0f);
        int moving = 0;
        if(rogue_input_is_down(&g_app.input, ROGUE_KEY_UP)) { g_app.player.base.pos.y -= speed * (float)g_app.dt; g_app.player.facing = 3; moving=1; }
        if(rogue_input_is_down(&g_app.input, ROGUE_KEY_DOWN)) { g_app.player.base.pos.y += speed * (float)g_app.dt; g_app.player.facing = 0; moving=1; }
        if(rogue_input_is_down(&g_app.input, ROGUE_KEY_LEFT)) { g_app.player.base.pos.x -= speed * (float)g_app.dt; g_app.player.facing = 1; moving=1; }
        if(rogue_input_is_down(&g_app.input, ROGUE_KEY_RIGHT)) { g_app.player.base.pos.x += speed * (float)g_app.dt; g_app.player.facing = 2; moving=1; }
        if(moving && g_app.player_state==0) g_app.player_state = 1; /* auto switch to walk */
        if(!moving) g_app.player_state = 0;
        /* Wrap inside map bounds */
        if(g_app.player.base.pos.x < 0) g_app.player.base.pos.x = 0;
        if(g_app.player.base.pos.y < 0) g_app.player.base.pos.y = 0;
        if(g_app.player.base.pos.x > g_app.world_map.width-1) g_app.player.base.pos.x = (float)(g_app.world_map.width-1);
        if(g_app.player.base.pos.y > g_app.world_map.height-1) g_app.player.base.pos.y = (float)(g_app.world_map.height-1);

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
    int frame_count = g_app.player_frame_count[g_app.player_state][anim_sheet_dir];
        if(frame_count <=0) frame_count = 1;
        if(g_app.player_state == 0){
            g_app.player.anim_frame = 0;
            g_app.player.anim_time = 0.0f;
        } else {
            int cur = g_app.player.anim_frame;
            int cur_dur = g_app.player_frame_time_ms[g_app.player_state][anim_sheet_dir][cur];
            if(cur_dur <=0) cur_dur = 120;
            if(g_app.player.anim_time >= (float)cur_dur){
                g_app.player.anim_time = 0.0f;
                g_app.player.anim_frame = (g_app.player.anim_frame + 1) % frame_count;
            }
        }

        /* Update camera to follow player (center) */
        g_app.cam_x = g_app.player.base.pos.x * g_app.tile_size - g_app.viewport_w / 2.0f;
        g_app.cam_y = g_app.player.base.pos.y * g_app.tile_size - g_app.viewport_h / 2.0f;
        if(g_app.cam_x < 0) g_app.cam_x = 0; if(g_app.cam_y < 0) g_app.cam_y = 0;
        float world_px_w = (float)g_app.world_map.width * g_app.tile_size;
        float world_px_h = (float)g_app.world_map.height * g_app.tile_size;
        if(g_app.cam_x > world_px_w - g_app.viewport_w) g_app.cam_x = world_px_w - g_app.viewport_w;
        if(g_app.cam_y > world_px_h - g_app.viewport_h) g_app.cam_y = world_px_h - g_app.viewport_h;

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
        if(g_app.player_loaded)
        {
            int dir = g_app.player.facing;
            /* Use side sheet for both left/right; flip if facing left */
            int sheet_dir = (dir==1 || dir==2)? 1 : dir; /* 0=down,1=side,3=up */
            const RogueSprite* spr = &g_app.player_frames[g_app.player_state][sheet_dir][g_app.player.anim_frame];
            /* Fallback: if chosen frame invalid, scan for first valid frame in current state/direction */
            if(!spr->sw){
                for(int f=0; f<8; f++){
                    if(g_app.player_frames[g_app.player_state][sheet_dir][f].sw){
                        spr = &g_app.player_frames[g_app.player_state][sheet_dir][f];
                        break;
                    }
                }
            }
            if(spr->sw && spr->tex && spr->tex->handle)
            {
                int px = (int)(g_app.player.base.pos.x * tsz * scale - g_app.cam_x);
                int py = (int)(g_app.player.base.pos.y * tsz * scale - g_app.cam_y);
#if defined(ROGUE_HAVE_SDL)
                if(dir==1) /* left: manual flip */
                {
                    SDL_Rect src = { spr->sx, spr->sy, spr->sw, spr->sh };
                    SDL_Rect dst = { px, py, spr->sw*scale, spr->sh*scale };
                    SDL_RenderCopyEx(g_app.renderer, spr->tex->handle, &src, &dst, 0.0, NULL, SDL_FLIP_HORIZONTAL);
                }
                else
                {
                    rogue_sprite_draw(spr, px, py, scale);
                }
                /* Optional debug: outline player bounding box if F1 held (future input hook) */
#endif
            }
            else {
                /* Fallback: draw placeholder if still no valid sprite */
#if defined(ROGUE_HAVE_SDL)
                SDL_SetRenderDrawColor(g_app.renderer, 255, 0, 255, 255);
                SDL_Rect pr = { (int)(g_app.player.base.pos.x*tsz*scale - g_app.cam_x), (int)(g_app.player.base.pos.y*tsz*scale - g_app.cam_y), g_app.player_frame_size*scale, g_app.player_frame_size*scale };
                SDL_RenderFillRect(g_app.renderer, &pr);
                /* On-screen debug list of failed sheets */
                int dy = 20; int line = 0;
                rogue_font_draw_text(4, dy + line*10, "Player sheets load status:",1,(RogueColor){255,255,0,255}); line++;
                const char* states[3] = {"idle","walk","run"};
                const char* dirs[4] = {"down","left","right","up"};
                for(int s=0;s<3;s++){
                    for(int d=0; d<4; d++){
                        if(!g_app.player_sheet_loaded[s][d]){
                            char buf[96];
                            snprintf(buf,sizeof buf,"%s-%s: FAIL", states[s], dirs[d]);
                            rogue_font_draw_text(4, dy + line*10, buf,1,(RogueColor){255,80,80,255});
                            line++;
                        } else if(line < 12){ /* show a few successes */
                            char buf[96];
                            snprintf(buf,sizeof buf,"%s-%s: OK", states[s], dirs[d]);
                            rogue_font_draw_text(4, dy + line*10, buf,1,(RogueColor){120,255,120,255});
                            line++;
                        }
                    }
                }
#endif
            }
        }
        else
        {
            SDL_SetRenderDrawColor(g_app.renderer, 255,255,255,255);
            SDL_Rect pr = { (int)(g_app.player.base.pos.x*tsz*scale), (int)(g_app.player.base.pos.y*tsz*scale), g_app.player_frame_size*scale, g_app.player_frame_size*scale };
            SDL_RenderFillRect(g_app.renderer, &pr);
        }

    /* Mini-map in corner (scaled down, render-target cached) */
    int mm_max_size = 240; /* limit minimap maximum dimension in pixels */
    float scale_w = (float)mm_max_size / (float)g_app.world_map.width;
    float scale_h = (float)mm_max_size / (float)g_app.world_map.height;
    float mm_scale_f = (scale_w < scale_h)? scale_w : scale_h;
    if(mm_scale_f < 0.5f) mm_scale_f = 0.5f; /* minimum visibility */
    int mm_scale = (int)mm_scale_f; if(mm_scale < 1) mm_scale = 1;
    int mm_w = g_app.world_map.width * mm_scale;
    int mm_h = g_app.world_map.height * mm_scale;
    if(mm_w > mm_max_size) mm_w = mm_max_size;
    if(mm_h > mm_max_size) mm_h = mm_max_size;
    int mm_x_off = g_app.viewport_w - mm_w - 8;
    int mm_y_off = 8;
    int step = (g_app.world_map.width > 500 || g_app.world_map.height > 500)? 2 : 1;
    ensure_minimap_target(mm_w, mm_h);
    redraw_minimap_if_needed(mm_w, mm_h, step);
#ifdef ROGUE_HAVE_SDL
    if(g_app.minimap_tex){
        SDL_Rect dst = { mm_x_off, mm_y_off, mm_w, mm_h };
            SDL_RenderCopy(g_app.renderer, g_app.minimap_tex, NULL, &dst); g_app.frame_draw_calls++;
        SDL_SetRenderDrawColor(g_app.renderer,255,255,255,255);
        int pmx = mm_x_off + (int)((g_app.player.base.pos.x / (float)g_app.world_map.width) * mm_w);
        int pmy = mm_y_off + (int)((g_app.player.base.pos.y / (float)g_app.world_map.height) * mm_h);
        SDL_Rect mmpr = { pmx, pmy, 2, 2 };
            SDL_RenderFillRect(g_app.renderer, &mmpr); g_app.frame_draw_calls++;
    }
#endif
    }
    /* FPS & world-gen overlay (uses last frame's fps value) */
    {
        int overlay_scale = (g_app.viewport_w >= 1400) ? 2 : 1;
        int line_h = (g_rogue_builtin_font.glyph_h * overlay_scale) + overlay_scale; /* crude spacing */
        int base_x = 4;
        int base_y = 4;
#ifdef ROGUE_HAVE_SDL
        /* Background panel for readability */
        if(g_app.renderer){
            int panel_w =  (overlay_scale==1? 520 : 600);
            int panel_h = line_h * 4 + 4;
            SDL_Rect bg = { base_x-2, base_y-2, panel_w, panel_h };
            SDL_SetRenderDrawColor(g_app.renderer, 0,0,0,120);
            SDL_RenderFillRect(g_app.renderer, &bg);
            g_app.frame_draw_calls++;
        }
#endif
        char fps_buf[48];
        snprintf(fps_buf, sizeof fps_buf, "FPS: %.1f", g_app.fps);
        rogue_font_draw_text(base_x, base_y, fps_buf, overlay_scale, (RogueColor){255,255,255,255});
        char m_buf[64];
        snprintf(m_buf, sizeof m_buf, "DRAWS:%d TILES:%d", g_app.frame_draw_calls, g_app.frame_tile_quads);
        rogue_font_draw_text(base_x, base_y + line_h, m_buf, overlay_scale, (RogueColor){220,220,220,255});
        char gen_buf1[96];
        snprintf(gen_buf1, sizeof gen_buf1, "WT:%.2f OCT:%d GAIN:%.2f LAC:%.2f", g_app.gen_water_level, g_app.gen_noise_octaves, g_app.gen_noise_gain, g_app.gen_noise_lacunarity);
        rogue_font_draw_text(base_x, base_y + line_h*2, gen_buf1, overlay_scale, (RogueColor){180,220,255,255});
        char gen_buf2[128];
        snprintf(gen_buf2, sizeof gen_buf2, "RIV:%d LEN:%d CAVE>%.2f F5/F6 WT F7/F8 OCT F9/F10 RIV F11/F12 GAIN ` REGEN", g_app.gen_river_sources, g_app.gen_river_max_length, g_app.gen_cave_thresh);
        rogue_font_draw_text(base_x, base_y + line_h*3, gen_buf2, overlay_scale, (RogueColor){160,200,240,255});
    }
    SDL_RenderPresent(g_app.renderer);
#endif
    rogue_game_loop_iterate();
    g_app.frame_count++;
    double frame_end = now_seconds();
    g_app.frame_ms = (frame_end - frame_start) * 1000.0;
    g_app.dt = (g_game_loop.target_frame_seconds > 0.0) ? g_game_loop.target_frame_seconds : (frame_end - frame_start);
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
}

int rogue_app_frame_count(void) { return g_app.frame_count; }

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
