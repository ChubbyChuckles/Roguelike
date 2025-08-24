/* MIT License
 * Platform / SDL initialization utilities extracted from app.c */
#include "platform.h"
#include "../util/log.h"
#include "app_state.h"

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

bool rogue_platform_init(const RogueAppConfig* cfg)
{
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
    if (cfg->allow_resize)
        win_flags |= SDL_WINDOW_RESIZABLE;
    g_app.window =
        SDL_CreateWindow(cfg->window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         cfg->window_width, cfg->window_height, win_flags);
    if (!g_app.window)
    {
        ROGUE_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    Uint32 rflags = SDL_RENDERER_ACCELERATED;
    if (cfg->enable_vsync)
        rflags |= SDL_RENDERER_PRESENTVSYNC;
    g_app.renderer = SDL_CreateRenderer(g_app.window, -1, rflags);
    if (!g_app.renderer)
    {
        ROGUE_LOG_WARN("SDL_CreateRenderer failed (%s). Headless mode enabled.", SDL_GetError());
        g_app.headless = 1;
    }
    extern SDL_Renderer* g_internal_sdl_renderer_ref; /* temporary exposure */
    g_internal_sdl_renderer_ref = g_app.renderer;
    if (cfg->logical_width > 0 && cfg->logical_height > 0)
    {
        SDL_RenderSetLogicalSize(g_app.renderer, cfg->logical_width, cfg->logical_height);
        if (cfg->integer_scale)
            SDL_RenderSetIntegerScale(g_app.renderer, SDL_TRUE);
    }
    rogue_platform_apply_window_mode();
#else
    (void) cfg;
#endif
    return true;
}

void rogue_platform_apply_window_mode(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.window)
        return;
    Uint32 flags = 0;
    switch (g_app.cfg.window_mode)
    {
    case ROGUE_WINDOW_FULLSCREEN:
        flags = SDL_WINDOW_FULLSCREEN;
        break;
    case ROGUE_WINDOW_BORDERLESS:
        flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
        break;
    case ROGUE_WINDOW_WINDOWED:
    default:
        flags = 0;
        break;
    }
    if (SDL_SetWindowFullscreen(g_app.window, flags) != 0)
    {
        ROGUE_LOG_WARN("Failed to set fullscreen mode: %s", SDL_GetError());
    }
#endif
}

void rogue_platform_shutdown(void)
{
#ifdef ROGUE_HAVE_SDL
    if (g_app.minimap_tex)
    {
        SDL_DestroyTexture(g_app.minimap_tex);
        g_app.minimap_tex = NULL;
    }
    if (g_app.renderer)
        SDL_DestroyRenderer(g_app.renderer);
    if (g_app.window)
        SDL_DestroyWindow(g_app.window);
    SDL_Quit();
#endif
}
