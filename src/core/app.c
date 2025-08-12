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
    int frame_count;
    double dt;
    double fps;
    double frame_ms;
    double avg_frame_ms_accum;
    int avg_frame_samples;
} RogueAppState;

static RogueAppState g_app;

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
        SDL_RenderSetLogicalSize(g_app.renderer, cfg->logical_width, cfg->logical_height);
        if (cfg->integer_scale) SDL_RenderSetIntegerScale(g_app.renderer, SDL_TRUE);
    }

    apply_window_mode();

    RogueGameLoopConfig loop_cfg = {.target_fps = cfg->target_fps};
    rogue_game_loop_init(&loop_cfg);
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
    SDL_SetRenderDrawColor(g_app.renderer, g_app.cfg.background_color.r, g_app.cfg.background_color.g,
                           g_app.cfg.background_color.b, g_app.cfg.background_color.a);
    SDL_RenderClear(g_app.renderer);
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
    if (!g_app.renderer) return;
    if (SDL_RenderSetVSync(enabled ? 1 : 0) != 0)
        ROGUE_LOG_WARN("Failed to set vsync: %s", SDL_GetError());
    else
        g_app.cfg.enable_vsync = enabled;
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
