/**
 * @file renderer.h
 * @brief Core rendering system for the roguelike game.
 *
 * Provides the primary rendering interface for the game, including renderer
 * initialization, basic drawing operations, and color management. This module
 * abstracts the underlying graphics API and provides a consistent interface
 * for all rendering operations throughout the game.
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
#ifndef ROGUE_GRAPHICS_RENDERER_H
#define ROGUE_GRAPHICS_RENDERER_H

/**
 * @brief Core renderer structure.
 *
 * Contains the state and data needed for the rendering system.
 * Currently minimal but designed to be extended with additional
 * rendering features as needed.
 */
typedef struct RogueRenderer
{
    int dummy; ///< Placeholder field until structure is expanded
} RogueRenderer;

/**
 * @brief RGBA color representation.
 *
 * Represents a color with red, green, blue, and alpha (transparency)
 * components. Each component ranges from 0-255, with 255 being full
 * intensity and 0 being no intensity/fully transparent (for alpha).
 */
typedef struct RogueColor
{
    unsigned char r; ///< Red component (0-255)
    unsigned char g; ///< Green component (0-255)
    unsigned char b; ///< Blue component (0-255)
    unsigned char a; ///< Alpha/transparency component (0-255, 255=opaque)
} RogueColor;

/**
 * @brief Initialize the renderer system.
 *
 * Sets up the renderer and prepares it for drawing operations.
 * Must be called before using any other renderer functions.
 *
 * @param r Pointer to the renderer structure to initialize
 * @return Non-zero on success, 0 on failure
 */
int rogue_renderer_init(RogueRenderer* r);

/**
 * @brief Shut down and clean up the renderer.
 *
 * Releases all resources used by the renderer and performs cleanup.
 * Should be called when the renderer is no longer needed.
 *
 * @param r Pointer to the renderer structure to shut down
 */
void rogue_renderer_shutdown(RogueRenderer* r);

/**
 * @brief Set the current drawing color.
 *
 * Sets the color that will be used for subsequent drawing operations
 * like clearing the screen or drawing primitives.
 *
 * @param r Pointer to the renderer
 * @param c The color to set as the current drawing color
 */
void rogue_renderer_set_draw_color(RogueRenderer* r, RogueColor c);

/**
 * @brief Clear the entire rendering surface.
 *
 * Fills the entire rendering surface with the current drawing color.
 * Typically called at the beginning of each frame to clear the previous
 * frame's contents.
 *
 * @param r Pointer to the renderer
 */
void rogue_renderer_clear(RogueRenderer* r);

/**
 * @brief Present the rendered frame to the display.
 *
 * Displays the current frame buffer to the screen. Should be called
 * after all drawing operations for the current frame are complete.
 * This typically swaps the back buffer with the front buffer.
 *
 * @param r Pointer to the renderer
 */
void rogue_renderer_present(RogueRenderer* r);

#endif
