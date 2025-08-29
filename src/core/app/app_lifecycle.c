/**
 * @file app_lifecycle.c
 * @brief Application lifecycle management and runtime controls.
 *
 * This file provides the core application lifecycle functions and runtime
 * controls that were extracted from the main app.c file. It handles the
 * main application loop, window management, performance metrics, and
 * various runtime state queries and controls.
 *
 * Key functionality includes:
 * - Main application run loop
 * - Window mode controls (fullscreen/windowed)
 * - Performance metrics and frame timing
 * - Game state queries (player health, enemy count, etc.)
 * - Start screen controls
 */

#include "../../game/game_loop.h" /* g_game_loop */
#include "../../game/platform.h"
#include "../../game/start_screen.h"
#include "../../util/metrics.h"
#include "app.h"
#include "app_state.h"
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

/**
 * @brief Get the current player health value.
 *
 * Returns the current health of the player character from the global
 * application state.
 *
 * @return int Current player health value.
 */
int rogue_app_player_health(void) { return g_app.player.health; }

/**
 * @brief Run the main application loop.
 *
 * Executes the main application loop, continuously calling rogue_app_step()
 * until the game loop is no longer running. This is the primary entry point
 * for the application's execution.
 */
void rogue_app_run(void)
{
    while (g_game_loop.running)
    {
        rogue_app_step();
    }
}

/**
 * @brief Get the current frame count.
 *
 * Returns the total number of frames that have been rendered since
 * the application started.
 *
 * @return int Total frame count.
 */
int rogue_app_frame_count(void) { return g_app.frame_count; }

/**
 * @brief Get the current enemy count.
 *
 * Returns the number of active enemies in the current game session.
 *
 * @return int Current enemy count.
 */
int rogue_app_enemy_count(void) { return g_app.enemy_count; }

/**
 * @brief Skip the start screen.
 *
 * Immediately hides the start screen and proceeds directly to the game.
 * This can be useful for automated testing or quick game startup.
 */
void rogue_app_skip_start_screen(void) { g_app.show_start_screen = 0; }

/**
 * @brief Toggle between fullscreen and windowed mode.
 *
 * Switches the application window between fullscreen and windowed modes.
 * Only available when SDL support is compiled in. The change takes effect
 * immediately by calling the platform's window mode application function.
 *
 * @note This function has no effect if SDL support is not available.
 */
void rogue_app_toggle_fullscreen(void)
{
#ifdef ROGUE_HAVE_SDL
    if (!g_app.window)
        return;
    if (g_app.cfg.window_mode == ROGUE_WINDOW_FULLSCREEN)
        g_app.cfg.window_mode = ROGUE_WINDOW_WINDOWED;
    else
        g_app.cfg.window_mode = ROGUE_WINDOW_FULLSCREEN;
    rogue_platform_apply_window_mode();
#endif
}

/**
 * @brief Set the vertical sync (V-Sync) state.
 *
 * Attempts to enable or disable vertical synchronization. Note that this
 * setting may not take effect immediately and might require renderer
 * recreation on some platforms.
 *
 * @param enabled 1 to enable V-Sync, 0 to disable.
 * @note Currently not dynamically supported without renderer recreation
 *       when SDL is available.
 */
void rogue_app_set_vsync(int enabled)
{
#ifdef ROGUE_HAVE_SDL
    (void) enabled; /* not dynamically supported without renderer recreation */
#else
    (void) enabled;
#endif
}

/**
 * @brief Get current performance metrics.
 *
 * Retrieves the current frames per second (FPS), current frame time in
 * milliseconds, and average frame time in milliseconds. All output
 * parameters are optional and can be NULL if not needed.
 *
 * @param out_fps Optional output for current FPS.
 * @param out_frame_ms Optional output for current frame time in milliseconds.
 * @param out_avg_frame_ms Optional output for average frame time in milliseconds.
 */
void rogue_app_get_metrics(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms)
{
    rogue_metrics_get(out_fps, out_frame_ms, out_avg_frame_ms);
}

/**
 * @brief Get the current delta time.
 *
 * Returns the time elapsed since the last frame in seconds. This is
 * typically used for frame-rate independent updates and animations.
 *
 * @return double Delta time in seconds.
 */
double rogue_app_delta_time(void) { return rogue_metrics_delta_time(); }
