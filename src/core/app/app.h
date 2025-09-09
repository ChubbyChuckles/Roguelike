/**
 * @file app.h
 * @brief Core application framework for the roguelike game.
 *
 * This header defines the main application configuration, initialization,
 * and lifecycle management functions. It provides the primary API for
 * setting up the game window, running the main loop, and managing core
 * application state including enemy spawning, player status, and visual
 * effects like damage numbers and hitstop.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

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
#ifndef ROGUE_CORE_APP_H
#define ROGUE_CORE_APP_H

#include "../../graphics/renderer.h"
#include <stdbool.h>

/**
 * @brief Window display modes for the application.
 *
 * Defines the different window modes that the application can use,
 * from windowed mode to fullscreen display options.
 */
typedef enum RogueWindowMode
{
    ROGUE_WINDOW_WINDOWED = 0, ///< Standard windowed mode with decorations
    ROGUE_WINDOW_BORDERLESS,   ///< Borderless window (fullscreen window)
    ROGUE_WINDOW_FULLSCREEN    ///< True fullscreen mode
} RogueWindowMode;

/* RogueColor defined in renderer.h */

/**
 * @brief Configuration structure for application initialization.
 *
 * Contains all the parameters needed to configure the application's
 * window, rendering settings, and initial display properties.
 */
typedef struct RogueAppConfig
{
    const char* window_title;    ///< Title bar text for the window
    int window_width;            ///< Initial physical window width in pixels
    int window_height;           ///< Initial physical window height in pixels
    int logical_width;           ///< Virtual/logical render width (e.g. 320 for pixel art)
    int logical_height;          ///< Virtual/logical render height (e.g. 180 for pixel art)
    int target_fps;              ///< Target frames per second (0 = uncapped busy loop)
    int enable_vsync;            ///< 1 = request vsync, 0 = no vsync
    int allow_resize;            ///< 1 = user can resize window, 0 = fixed size
    int integer_scale;           ///< 1 = force integer scaling for crisp pixel art
    RogueWindowMode window_mode; ///< Initial window display mode
    RogueColor background_color; ///< Clear color for the rendering surface
} RogueAppConfig;

/**
 * @brief Initialize the application with the given configuration.
 *
 * Sets up the application's window, renderer, and core systems using
 * the provided configuration. Must be called before any other app functions.
 *
 * @param cfg Configuration structure containing window and render settings
 * @return true on successful initialization, false on failure
 */
bool rogue_app_init(const RogueAppConfig* cfg);

/**
 * @brief Run the main application loop until exit is requested.
 *
 * Executes the main game loop, handling input, updating game state,
 * and rendering frames continuously until the user requests to exit
 * or an error occurs.
 */
void rogue_app_run(void);

/**
 * @brief Execute exactly one frame of the application.
 *
 * Processes one complete frame of input, update, and render cycles.
 * Primarily used for testing and debugging tools that need precise
 * frame-by-frame control.
 */
void rogue_app_step(void);

/**
 * @brief Get the total number of frames processed since initialization.
 *
 * @return The cumulative frame count since rogue_app_init() was called
 */
int rogue_app_frame_count(void);

/**
 * @brief Clean up and shut down the application.
 *
 * Releases all resources, closes the window, and performs cleanup
 * of all initialized systems. Should be called before program exit.
 */
void rogue_app_shutdown(void);

/**
 * @brief Get the current count of live enemies.
 *
 * Returns the number of active enemy entities in the game world.
 * Used for telemetry, testing, and gameplay balancing.
 *
 * @return Current number of active enemies
 */
int rogue_app_enemy_count(void);

/**
 * @brief Skip the title screen and go directly to gameplay.
 *
 * Forces the application to bypass the start screen and jump
 * directly into the main game. Primarily used for automated
 * testing scenarios.
 */
void rogue_app_skip_start_screen(void);

/**
 * @brief Get the current player's health value.
 *
 * Returns the player's current health points for testing
 * and debugging purposes.
 *
 * @return Current player health value
 */
int rogue_app_player_health(void);

/* Test helper functions */
struct RogueEnemy; /* forward declaration */

/**
 * @brief Spawn a hostile enemy at the specified world position.
 *
 * Creates a new enemy entity at the given world coordinates for
 * testing purposes. The enemy will be hostile and engage the player.
 *
 * @param x X coordinate in world tile units
 * @param y Y coordinate in world tile units
 * @return Pointer to the spawned enemy, or NULL if spawning failed
 */
struct RogueEnemy* rogue_test_spawn_hostile_enemy(float x, float y);

/**
 * @brief Create a floating damage number at the specified position.
 *
 * Spawns a visual damage indicator that floats upward from the given
 * world coordinates, displaying the damage amount with appropriate
 * styling based on the damage source.
 *
 * @param x X coordinate in world tile units
 * @param y Y coordinate in world tile units
 * @param amount Damage amount to display
 * @param from_player 1 if damage dealt by player, 0 if taken by player
 */
void rogue_add_damage_number(float x, float y, int amount, int from_player);

/**
 * @brief Create a floating damage number with critical hit styling.
 *
 * Extended version of rogue_add_damage_number that supports critical
 * hit indication with special visual effects.
 *
 * @param x X coordinate in world tile units
 * @param y Y coordinate in world tile units
 * @param amount Damage amount to display
 * @param from_player 1 if damage dealt by player, 0 if taken by player
 * @param crit 1 if this is a critical hit, 0 for normal damage
 */
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit);

/**
 * @brief Get the count of currently active floating damage numbers.
 *
 * Returns the number of damage number visual effects currently
 * being displayed and animated.
 *
 * @return Number of active damage number effects
 */
int rogue_app_damage_number_count(void);

/**
 * @brief Add a hitstop/freeze frame effect.
 *
 * Adds a brief pause to the game action for dramatic effect,
 * typically used when landing critical hits or taking damage.
 * The effect duration should be brief to avoid disrupting gameplay.
 *
 * @param ms Duration of the hitstop effect in milliseconds
 */
void rogue_app_add_hitstop(float ms);

/**
 * @brief Force aging of damage numbers for testing.
 *
 * Advances the decay timer of all active damage numbers by the
 * specified amount. Used in tests to simulate the passage of time
 * without waiting for real-time decay.
 *
 * @param ms Milliseconds to advance damage number decay timers
 */
void rogue_app_test_decay_damage_numbers(float ms);

/* Dynamic controls */
void rogue_app_toggle_fullscreen(void);
void rogue_app_set_vsync(int enabled);

/* Frame metrics (updated each frame). Any pointer may be NULL. */
void rogue_app_get_metrics(double* out_fps, double* out_frame_ms, double* out_avg_frame_ms);

/* Time since last frame in seconds (clamped). */
double rogue_app_delta_time(void);

#endif
