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
#include "input/input.h"

void rogue_input_clear(RogueInputState* st)
{
    for (int i = 0; i < ROGUE_KEY_COUNT; ++i)
        st->keys[i] = false;
}

void rogue_input_apply_direction(RogueInputState* st, int dx, int dy)
{
    st->keys[ROGUE_KEY_UP] = dy < 0;
    st->keys[ROGUE_KEY_DOWN] = dy > 0;
    st->keys[ROGUE_KEY_LEFT] = dx < 0;
    st->keys[ROGUE_KEY_RIGHT] = dx > 0;
}

bool rogue_input_is_down(const RogueInputState* st, RogueKey key)
{
    if (key < 0 || key >= ROGUE_KEY_COUNT)
        return false;
    return st->keys[key];
}

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
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
    case SDL_SCANCODE_RETURN:
        *out_key = ROGUE_KEY_ACTION;
        return true;
    default:
        return false;
    }
}

void rogue_input_process_sdl_event(RogueInputState* st, const SDL_Event* ev)
{
    if (ev->type == SDL_KEYDOWN || ev->type == SDL_KEYUP)
    {
        RogueKey key;
        if (rogue_input_map_scancode(ev->key.keysym.scancode, &key))
        {
            st->keys[key] = (ev->type == SDL_KEYDOWN);
        }
    }
}
#endif
