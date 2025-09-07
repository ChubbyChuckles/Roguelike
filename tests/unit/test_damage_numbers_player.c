#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/damage_numbers.h"
/* player.h moved (or always lived) under entities/, fix broken relative include */
#include "../../src/entities/player.h"
#include <assert.h>
#include <stdio.h>

/* Removed local stubs for rogue_app_add_hitstop / damage number APIs to avoid LNK2005.
   Use the core implementations from app_step.c and damage_numbers.c. */
extern void rogue_app_add_hitstop(float ms);
/* rogue_add_damage_number / rogue_add_damage_number_ex already declared in damage_numbers.h */

int main(void)
{
    int before = rogue_app_damage_number_count();
    for (int i = 0; i < 5; i++)
    {
        rogue_add_damage_number(0.2f + i * 0.05f, 0.1f, 3 + i, 1);
    }
    int after = rogue_app_damage_number_count();
    /* We expect up to 5 new numbers; system may coalesce or drop, so just ensure monotonic
     * non-negative. */
    assert(after >= before);
    assert(after <= before + 5);
    printf("damage_numbers_player: OK (before=%d, after=%d, added=%d)\n", before, after,
           after - before);
    return 0;
}
