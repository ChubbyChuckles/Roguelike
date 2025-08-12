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

typedef enum RogueKey
{
    ROGUE_KEY_UP,
    ROGUE_KEY_DOWN,
    ROGUE_KEY_LEFT,
    ROGUE_KEY_RIGHT,
    ROGUE_KEY_ACTION,
    ROGUE_KEY_COUNT
} RogueKey;

typedef struct RogueInputState
{
    bool keys[ROGUE_KEY_COUNT];
} RogueInputState;

void rogue_input_clear(RogueInputState* st);
/* Update directional key state via vector components (-1,0,1) for tests / AI */
void rogue_input_apply_direction(RogueInputState* st, int dx, int dy);
/* Query convenience */
bool rogue_input_is_down(const RogueInputState* st, RogueKey key);

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
/* Translate SDL scancode into internal key (returns false if unmapped) */
bool rogue_input_map_scancode(int scancode, RogueKey* out_key);
/* Apply an SDL_Event keyboard state change */
void rogue_input_process_sdl_event(RogueInputState* st, const SDL_Event* ev);
#endif

#endif
