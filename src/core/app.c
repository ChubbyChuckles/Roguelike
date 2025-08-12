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
    RogueTexture tileset_tex;
    int tileset_loaded;
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
    g_app.tileset_loaded = 0; /* user will load externally later */
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
        /* Simple visualization of generated world (colored pixels / rectangles) */
    int scale = 16; /* scale up tiles for main view */
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
                SDL_Rect r = { x*scale, y*scale, scale, scale };
                SDL_RenderFillRect(g_app.renderer, &r);
            }
        }
        /* Player (placeholder rectangle). Later replaced by sprite frames. */
        SDL_SetRenderDrawColor(g_app.renderer, 255,255,255,255);
        SDL_Rect pr = { (int)(g_app.player.base.pos.x*scale), (int)(g_app.player.base.pos.y*scale), scale, scale };
        SDL_RenderFillRect(g_app.renderer, &pr);

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
