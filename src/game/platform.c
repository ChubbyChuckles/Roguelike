/**
 * @file platform.c
 * @brief Platform abstraction layer for SDL initialization and window management.
 *
 * This module provides platform-specific initialization and management functions
 * for the roguelike game, primarily handling SDL setup, window creation, and
 * renderer configuration. It abstracts platform differences and provides a
 * unified interface for graphics and input initialization.
 *
 * Key Features:
 * - SDL initialization with configurable subsystems (video, audio, events)
 * - Window creation with customizable size, title, and display modes
 * - Renderer setup with hardware acceleration and vsync support
 * - Logical rendering size and integer scaling support
 * - Fullscreen/windowed mode switching
 * - Graceful fallback to headless mode if rendering fails
 * - Proper resource cleanup and shutdown
 *
 * @author Game Development Team
 * @date 2025
 */

/* MIT License
 * Platform / SDL initialization utilities extracted from app.c */
#include "platform.h"
#include "../core/app/app_state.h"
#include "../util/log.h"

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/**
 * @brief Initializes the platform layer and SDL subsystems.
 *
 * Sets up the core platform components including SDL initialization, window creation,
 * and renderer setup. Configures SDL subsystems based on available features and
 * user preferences. Handles graceful fallback to headless mode if rendering fails.
 *
 * @param cfg Pointer to application configuration structure
 * @return true if initialization successful, false on failure
 *
 * @note Initializes SDL with VIDEO and EVENTS subsystems by default
 * @note Adds AUDIO subsystem if SDL_mixer is available
 * @note Creates window with configurable size, title, and resizable flag
 * @note Sets up hardware-accelerated renderer with optional vsync
 * @note Falls back to headless mode if renderer creation fails
 * @note Configures logical rendering size and integer scaling if specified
 * @note Applies initial window mode settings
 */
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

/**
 * @brief Applies the current window mode setting to the SDL window.
 *
 * Updates the window display mode based on the configured window mode setting.
 * Supports fullscreen, borderless fullscreen, and windowed modes. Handles
 * SDL fullscreen API calls and provides error logging for failures.
 *
 * @note Only operates if SDL window exists
 * @note Uses SDL_WINDOW_FULLSCREEN for exclusive fullscreen
 * @note Uses SDL_WINDOW_FULLSCREEN_DESKTOP for borderless fullscreen
 * @note Logs warnings if fullscreen mode changes fail
 * @note Safe to call multiple times to reapply current mode
 */
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

/**
 * @brief Shuts down the platform layer and cleans up SDL resources.
 *
 * Performs proper cleanup of all SDL resources in reverse order of creation.
 * Destroys textures, renderer, and window, then shuts down SDL subsystems.
 * Ensures memory is freed and system resources are properly released.
 *
 * @note Destroys minimap texture if it exists
 * @note Destroys renderer before window to prevent invalid references
 * @note Calls SDL_Quit() to shut down all SDL subsystems
 * @note Safe to call even if some resources are NULL
 * @note Should be called during application shutdown
 */
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
