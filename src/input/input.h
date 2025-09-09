/**
 * @file input.h
 * @brief Input handling system for the roguelike game.
 *
 * Provides cross-platform input handling with support for keyboard input,
 * text input, and integration with SDL events. The system uses an internal
 * key mapping that abstracts away platform-specific input codes and
 * provides convenient query functions for game logic.
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
#ifndef ROGUE_INPUT_INPUT_H
#define ROGUE_INPUT_INPUT_H

#include <stdbool.h>

/**
 * @brief Game-specific key enumeration.
 *
 * Defines the logical keys used by the game, abstracting away
 * platform-specific key codes. This allows the game logic to
 * work with semantic key meanings rather than raw input codes.
 */
typedef enum RogueKey
{
    ROGUE_KEY_UP,       ///< Move up / navigate up in menus
    ROGUE_KEY_DOWN,     ///< Move down / navigate down in menus
    ROGUE_KEY_LEFT,     ///< Move left / navigate left in menus
    ROGUE_KEY_RIGHT,    ///< Move right / navigate right in menus
    ROGUE_KEY_ACTION,   ///< Primary action key (attack/interact - typically SPACE)
    ROGUE_KEY_DIALOGUE, ///< Dialogue confirm/advance (typically ENTER)
    ROGUE_KEY_CANCEL,   ///< Cancel/back key (typically ESC or controller B)
    ROGUE_KEY_COUNT     ///< Total number of keys (used for array sizing)
} RogueKey;

/**
 * @brief Input state structure.
 *
 * Maintains the current and previous frame input state for all tracked
 * keys, enabling detection of key presses, releases, and held states.
 * Also includes a text input buffer for handling text entry.
 */
typedef struct RogueInputState
{
    bool keys[ROGUE_KEY_COUNT];      ///< Current frame key states (true = pressed)
    bool prev_keys[ROGUE_KEY_COUNT]; ///< Previous frame key states for edge detection
    char text_buffer[64];            ///< Text input buffer for typing
    int text_len;                    ///< Current length of text in buffer
} RogueInputState;

/**
 * @brief Clear all input state.
 *
 * Resets all key states and clears the text input buffer.
 * Useful for initialization or when switching input contexts.
 *
 * @param st Pointer to the input state to clear
 */
void rogue_input_clear(RogueInputState* st);

/**
 * @brief Update directional key state programmatically.
 *
 * Sets the directional key states based on vector components.
 * Useful for testing, AI input simulation, or gamepad analog stick mapping.
 *
 * @param st Pointer to the input state to update
 * @param dx Horizontal direction (-1 = left, 0 = none, 1 = right)
 * @param dy Vertical direction (-1 = up, 0 = none, 1 = down)
 */
void rogue_input_apply_direction(RogueInputState* st, int dx, int dy);

/**
 * @brief Check if a key is currently held down.
 *
 * Returns true if the specified key is currently pressed (held down).
 * This state persists across multiple frames while the key remains pressed.
 *
 * @param st Pointer to the input state to query
 * @param key The key to check
 * @return true if the key is currently pressed, false otherwise
 */
bool rogue_input_is_down(const RogueInputState* st, RogueKey key);

/**
 * @brief Check if a key was just pressed this frame.
 *
 * Returns true only on the frame when a key transitions from released
 * to pressed. This is useful for detecting single key press events
 * rather than continuous held states.
 *
 * @param st Pointer to the input state to query
 * @param key The key to check
 * @return true if the key was just pressed this frame, false otherwise
 */
bool rogue_input_was_pressed(const RogueInputState* st, RogueKey key);

/**
 * @brief Advance input state to the next frame.
 *
 * Copies current key states to previous key states in preparation
 * for the next frame's input processing. Should be called once
 * per frame after all input processing is complete.
 *
 * @param st Pointer to the input state to advance
 */
void rogue_input_next_frame(RogueInputState* st);

/**
 * @brief Add a character to the text input buffer.
 *
 * Appends a character to the text input buffer if there is space
 * available. Used for handling text input events like typing in
 * chat or name entry fields.
 *
 * @param st Pointer to the input state
 * @param c Character to add to the text buffer
 */
void rogue_input_push_char(RogueInputState* st, char c);

#ifdef ROGUE_HAVE_SDL
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

/**
 * @brief Map SDL scancode to game key.
 *
 * Converts an SDL scancode (raw keyboard key identifier) to the
 * corresponding game-specific key enumeration. This provides the
 * abstraction layer between platform input and game logic.
 *
 * @param scancode SDL scancode to map
 * @param out_key Pointer to store the mapped game key
 * @return true if the scancode was successfully mapped, false if unmapped
 */
bool rogue_input_map_scancode(int scancode, RogueKey* out_key);

/**
 * @brief Process an SDL event and update input state.
 *
 * Handles SDL keyboard and text input events, updating the input
 * state accordingly. This is the primary integration point between
 * SDL's event system and the game's input handling.
 *
 * @param st Pointer to the input state to update
 * @param ev Pointer to the SDL event to process
 */
void rogue_input_process_sdl_event(RogueInputState* st, const SDL_Event* ev);
#endif

#endif
