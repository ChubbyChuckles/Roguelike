
/**
 * @file input.c
 * @brief Input state management system for the roguelike game, handling keyboard,
 *        controller, and text input with frame-based state tracking.
 */

#include "input.h"

/**
 * @brief Clears all key states and text input in the input state.
 *
 * Resets both current and previous key states to false and clears the text buffer.
 *
 * @param st The input state to clear.
 */
void rogue_input_clear(RogueInputState* st)
{
    for (int i = 0; i < ROGUE_KEY_COUNT; ++i)
        st->keys[i] = false;
    for (int i = 0; i < ROGUE_KEY_COUNT; ++i)
        st->prev_keys[i] = false;
    st->text_len = 0;
}

/**
 * @brief Applies directional input by setting the appropriate directional keys.
 *
 * Maps dx/dy values to UP/DOWN/LEFT/RIGHT key states. Negative dx/dy values
 * set the corresponding directional keys to true.
 *
 * @param st The input state to modify.
 * @param dx Horizontal direction (-1 for left, 1 for right, 0 for neutral).
 * @param dy Vertical direction (-1 for up, 1 for down, 0 for neutral).
 */
void rogue_input_apply_direction(RogueInputState* st, int dx, int dy)
{
    st->keys[ROGUE_KEY_UP] = dy < 0;
    st->keys[ROGUE_KEY_DOWN] = dy > 0;
    st->keys[ROGUE_KEY_LEFT] = dx < 0;
    st->keys[ROGUE_KEY_RIGHT] = dx > 0;
}

/**
 * @brief Checks if a specific key is currently being held down.
 *
 * @param st The input state to check.
 * @param key The key to check.
 * @return true if the key is down, false otherwise or if key is invalid.
 */
bool rogue_input_is_down(const RogueInputState* st, RogueKey key)
{
    if (key < 0 || key >= ROGUE_KEY_COUNT)
        return false;
    return st->keys[key];
}

/**
 * @brief Checks if a specific key was just pressed this frame.
 *
 * A key press is detected when the key is down in the current frame but was
 * not down in the previous frame.
 *
 * @param st The input state to check.
 * @param key The key to check.
 * @return true if the key was just pressed, false otherwise or if key is invalid.
 */
bool rogue_input_was_pressed(const RogueInputState* st, RogueKey key)
{
    if (key < 0 || key >= ROGUE_KEY_COUNT)
        return false;
    return st->keys[key] && !st->prev_keys[key];
}

/**
 * @brief Advances the input state to the next frame.
 *
 * Copies current key states to previous states and clears the text buffer
 * for the new frame. This should be called once per game frame.
 *
 * @param st The input state to advance.
 */
void rogue_input_next_frame(RogueInputState* st)
{
    for (int i = 0; i < ROGUE_KEY_COUNT; ++i)
        st->prev_keys[i] = st->keys[i];
    st->text_len = 0; /* clear typed chars each frame consumer can copy if needed */
}

/**
 * @brief Adds a character to the input state's text buffer.
 *
 * Appends the character to the text buffer if there's space available.
 * The buffer is null-terminated automatically.
 *
 * @param st The input state to modify.
 * @param c The character to add to the text buffer.
 */
void rogue_input_push_char(RogueInputState* st, char c)
{
    if (st->text_len < (int) (sizeof st->text_buffer) - 1)
    {
        st->text_buffer[st->text_len++] = c;
        st->text_buffer[st->text_len] = '\0';
    }
}

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>

/**
 * @brief Maps an SDL scancode to a RogueKey enum value.
 *
 * Supports WASD movement keys, arrow keys, space for action, enter for dialogue,
 * and escape for cancel. Multiple SDL scancodes can map to the same RogueKey.
 *
 * @param scancode The SDL scancode to map.
 * @param out_key Pointer to store the mapped RogueKey.
 * @return true if the scancode was mapped successfully, false otherwise.
 */
bool rogue_input_map_scancode(int scancode, RogueKey* out_key)
{
    switch (scancode)
    {
    case SDL_SCANCODE_W:
    case SDL_SCANCODE_UP:
        *out_key = ROGUE_KEY_UP;
        return true;
    case SDL_SCANCODE_S:
    case SDL_SCANCODE_DOWN:
        *out_key = ROGUE_KEY_DOWN;
        return true;
    case SDL_SCANCODE_A:
    case SDL_SCANCODE_LEFT:
        *out_key = ROGUE_KEY_LEFT;
        return true;
    case SDL_SCANCODE_D:
    case SDL_SCANCODE_RIGHT:
        *out_key = ROGUE_KEY_RIGHT;
        return true;
    case SDL_SCANCODE_SPACE:
        *out_key = ROGUE_KEY_ACTION; /* attack */
        return true;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        *out_key = ROGUE_KEY_DIALOGUE; /* accept/confirm */
        return true;
    case SDL_SCANCODE_ESCAPE:
        *out_key = ROGUE_KEY_CANCEL; /* back/cancel */
        return true;
    default:
        return false;
    }
}

/**
 * @brief Processes an SDL event and updates the input state accordingly.
 *
 * Handles keyboard events by mapping SDL scancodes to RogueKey values and
 * processing text input. Also handles controller button events for SDL 2.0+
 * with basic button mappings for dialogue, cancel, and directional movement.
 *
 * @param st The input state to update.
 * @param ev The SDL event to process.
 */
void rogue_input_process_sdl_event(RogueInputState* st, const SDL_Event* ev)
{
    if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP)
    {
        RogueKey key;
        if (rogue_input_map_scancode(ev->key.keysym.scancode, &key))
        {
            st->keys[key] = (ev->type == SDL_KEYDOWN);
        }
        if (ev->type == SDL_KEYDOWN)
        {
            /* text input for letters/numbers basic */
            if (ev->key.keysym.sym >= 32 && ev->key.keysym.sym < 127)
                rogue_input_push_char(st, (char) ev->key.keysym.sym);
        }
    }
    /* Basic controller to key mapping via SDL controller events when available. */
#if SDL_VERSION_ATLEAST(2, 0, 0)
    else if (ev->type == SDL_CONTROLLERBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONUP)
    {
        bool down = (ev->type == SDL_CONTROLLERBUTTONDOWN);
        switch (ev->cbutton.button)
        {
        case SDL_CONTROLLER_BUTTON_A:
            st->keys[ROGUE_KEY_DIALOGUE] = down; /* accept */
            break;
        case SDL_CONTROLLER_BUTTON_B:
            st->keys[ROGUE_KEY_CANCEL] = down; /* back */
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            st->keys[ROGUE_KEY_UP] = down;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            st->keys[ROGUE_KEY_DOWN] = down;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            st->keys[ROGUE_KEY_LEFT] = down;
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            st->keys[ROGUE_KEY_RIGHT] = down;
            break;
        default:
            break;
        }
    }
#endif
}
#endif
