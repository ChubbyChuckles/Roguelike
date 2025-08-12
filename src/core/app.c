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

    /* Pre-generate world */
    RogueWorldGenConfig wcfg = { .seed = 1337u, .width = 80, .height = 60, .biome_regions = 10, .cave_iterations = 3, .cave_fill_chance = 0.45, .river_attempts = 2 };
    rogue_world_generate(&g_app.world_map, &wcfg);
    rogue_player_init(&g_app.player);
    g_app.tileset_loaded = 0; /* registry not finalized yet */
    g_app.tile_size = 16; /* terrain tiles */
    g_app.player_frame_size = 64;
    g_app.player_loaded = 0;
    g_app.player_state = 0;
    g_app.player_sheet_paths_loaded = 0;
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
        }
        if (ev.type == SDL_KEYDOWN && g_app.show_start_screen)
        {
            if (g_app.entering_seed)
            {
                if (ev.key.keysym.sym == SDLK_RETURN)
                {
                    /* regenerate */
                    rogue_tilemap_free(&g_app.world_map);
                    RogueWorldGenConfig wcfg = { .seed = g_app.pending_seed, .width = 80, .height = 60, .biome_regions = 10, .cave_iterations = 3, .cave_fill_chance = 0.45, .river_attempts = 2 };
                    rogue_world_generate(&g_app.world_map, &wcfg);
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
            rogue_tile_sprites_finalize();
            g_app.tileset_loaded = 1;
        }
        if(!g_app.player_loaded)
        {
            const char* state_names[3]  = {"idle","walk","run"};
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
            for(int s=0;s<3;s++)
            {
                for(int d=0; d<4; d++)
                {
                    const char* path = g_app.player_sheet_path[s][d];
                    if(rogue_texture_load(&g_app.player_tex[s][d], path))
                    {
                        int frames = g_app.player_tex[s][d].w / g_app.player_frame_size; if(frames>8) frames = 8;
                        g_app.player_frame_count[s][d] = frames;
                        for(int f=0; f<frames; f++)
                        {
                            g_app.player_frames[s][d][f].tex = &g_app.player_tex[s][d];
                            g_app.player_frames[s][d][f].sx = f * g_app.player_frame_size;
                            g_app.player_frames[s][d][f].sy = 0;
                            g_app.player_frames[s][d][f].sw = g_app.player_frame_size;
                            g_app.player_frames[s][d][f].sh = g_app.player_frame_size;
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
                }
            }
            g_app.player_loaded = 1;
            load_player_anim_config("assets/player_anim.cfg");
        }

        /* Update player position from input (simple) */
        float speed = (g_app.player_state==2)? 6.0f : (g_app.player_state==1? 3.0f : 0.0f);
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

        /* Advance animation */
        /* Advance animation with per-frame timing */
        g_app.player.anim_time += (float)g_app.dt * 1000.0f; /* ms */
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

        /* Render tiles */
        int scale = 1; /* 1 screen pixel per tile pixel until camera implemented */
    int tsz = g_app.tile_size;
    if(g_app.tileset_loaded)
        {
            for(int y=0;y<g_app.world_map.height;y++)
            for(int x=0;x<g_app.world_map.width;x++)
            {
                unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                if(t < ROGUE_TILE_MAX){
                    const RogueSprite* spr = rogue_tile_sprite_get_xy((RogueTileType)t, x, y);
                    if(spr && spr->sw) rogue_sprite_draw(spr, x*tsz*scale, y*tsz*scale, scale);
                }
            }
        }
        else
        {
            for(int y=0;y<g_app.world_map.height;y++)
            for(int x=0;x<g_app.world_map.width;x++)
            {
                unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                RogueColor c = { (unsigned char)(t*20), (unsigned char)(t*15), (unsigned char)(t*10), 255};
                SDL_SetRenderDrawColor(g_app.renderer, c.r,c.g,c.b,c.a);
                SDL_Rect r = { x*tsz*scale, y*tsz*scale, tsz*scale, tsz*scale };
                SDL_RenderFillRect(g_app.renderer, &r);
            }
        }

        /* Render player */
        if(g_app.player_loaded)
        {
            int dir = g_app.player.facing;
            /* Use side sheet for both left/right; flip if facing left */
            int sheet_dir = (dir==1 || dir==2)? 1 : dir; /* 0=down,1=side,3=up */
            const RogueSprite* spr = &g_app.player_frames[g_app.player_state][sheet_dir][g_app.player.anim_frame];
            if(spr->sw)
            {
                int px = (int)(g_app.player.base.pos.x * tsz * scale);
                int py = (int)(g_app.player.base.pos.y * tsz * scale);
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
#endif
            }
        }
        else
        {
            SDL_SetRenderDrawColor(g_app.renderer, 255,255,255,255);
            SDL_Rect pr = { (int)(g_app.player.base.pos.x*tsz*scale), (int)(g_app.player.base.pos.y*tsz*scale), g_app.player_frame_size*scale, g_app.player_frame_size*scale };
            SDL_RenderFillRect(g_app.renderer, &pr);
        }

        /* Mini-map in corner (scaled down) */
    int mm_scale = 2;
    int mm_x_off = 1920 - g_app.world_map.width*mm_scale - 10;
    int mm_y_off = 10;
        for(int y=0;y<g_app.world_map.height;y++)
        {
            for(int x=0;x<g_app.world_map.width;x++)
            {
                unsigned char t = g_app.world_map.tiles[y*g_app.world_map.width + x];
                RogueColor c = {0,0,0,255};
                switch(t){
                    case ROGUE_TILE_WATER: c=(RogueColor){30,90,200,255}; break;
                    case ROGUE_TILE_RIVER: c=(RogueColor){50,140,230,255}; break;
                    case ROGUE_TILE_GRASS: c=(RogueColor){40,160,60,255}; break;
                    case ROGUE_TILE_FOREST: c=(RogueColor){10,90,20,255}; break;
                    case ROGUE_TILE_MOUNTAIN: c=(RogueColor){120,120,120,255}; break;
                    case ROGUE_TILE_CAVE_WALL: c=(RogueColor){60,60,60,255}; break;
                    case ROGUE_TILE_CAVE_FLOOR: c=(RogueColor){110,80,60,255}; break;
                    default: break;
                }
                SDL_SetRenderDrawColor(g_app.renderer, c.r,c.g,c.b,c.a);
                SDL_Rect r = { mm_x_off + x*mm_scale, mm_y_off + y*mm_scale, mm_scale, mm_scale };
                SDL_RenderFillRect(g_app.renderer, &r);
            }
        }
        /* Player marker on minimap */
        SDL_SetRenderDrawColor(g_app.renderer,255,255,255,255);
    SDL_Rect mmpr = { mm_x_off + (int)(g_app.player.base.pos.x*mm_scale), mm_y_off + (int)(g_app.player.base.pos.y*mm_scale), mm_scale, mm_scale };
        SDL_RenderFillRect(g_app.renderer, &mmpr);
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
    if (g_app.renderer)
        SDL_DestroyRenderer(g_app.renderer);
    if (g_app.window)
        SDL_DestroyWindow(g_app.window);
    SDL_Quit();
#endif
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
