/**
 * @file main.c
 * @brief Entry point for the roguelike game application.
 *
 * This is the main program file that initializes the application configuration,
 * sets up logging from environment variables, initializes the app with window
 * settings, runs the main game loop, and handles shutdown. It includes the MIT
 * license header and configures basic app parameters like window title, size,
 * and target FPS.
 *
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
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
LIABILITY, WHETHER IN AN ACTION OR CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "core/app/app.h"
#include "util/log.h"

/**
 * @brief Main entry point for the roguelike game.
 *
 * Initializes logging from environment variables early, sets up the application
 * configuration with default window title "Roguelike", resolution 1920x1080,
 * and target 60 FPS. Calls rogue_app_init to start the app; logs an error and
 * exits with code 1 on failure. Runs the main game loop with rogue_app_run,
 * then shuts down with rogue_app_shutdown. Includes a comment to disable trunk
 * collision globally for player/entities (placeholder or TODO).
 *
 * @return int 0 on successful execution and shutdown, 1 on initialization failure.
 * @details
 * - Honor env-based logging level as early as possible using rogue_log_set_level_from_env().
 * - RogueAppConfig cfg = { .window_title = "Roguelike", .window_width = 1920, .window_height =
 * 1080, .target_fps = 60 };
 * - Disable trunk collision for player/entities globally (inline comment preserved).
 * @note The main function is the standard C entry point and does not take arguments.
 * @warning Ensure app.h and log.h are included; failure in init leads to immediate exit.
 */
int main(void)
{
    /* Honor env-based logging level as early as possible. */
    rogue_log_set_level_from_env();
    RogueAppConfig cfg = {
        .window_title = "Roguelike", .window_width = 1920, .window_height = 1080, .target_fps = 60};
    if (!rogue_app_init(&cfg))
    {
        ROGUE_LOG_ERROR("Failed to initialize app");
        return 1;
    }
    /* Disable trunk collision for player/entities globally. */
    rogue_app_run();
    rogue_app_shutdown();
    return 0;
}
