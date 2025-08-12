#ifndef ROGUE_INPUT_INPUT_H
#define ROGUE_INPUT_INPUT_H

#include <stdbool.h>

typedef enum RogueKey {
    ROGUE_KEY_UP,
    ROGUE_KEY_DOWN,
    ROGUE_KEY_LEFT,
    ROGUE_KEY_RIGHT,
    ROGUE_KEY_ACTION,
    ROGUE_KEY_COUNT
} RogueKey;

typedef struct RogueInputState {
    bool keys[ROGUE_KEY_COUNT];
} RogueInputState;

void rogue_input_clear(RogueInputState *st);
/* Update directional key state via vector components (-1,0,1) for tests / AI */
void rogue_input_apply_direction(RogueInputState *st, int dx, int dy);
/* Query convenience */
bool rogue_input_is_down(const RogueInputState *st, RogueKey key);

#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
/* Translate SDL scancode into internal key (returns false if unmapped) */
bool rogue_input_map_scancode(int scancode, RogueKey *out_key);
/* Apply an SDL_Event keyboard state change */
void rogue_input_process_sdl_event(RogueInputState *st, const SDL_Event *ev);
#endif

#endif
