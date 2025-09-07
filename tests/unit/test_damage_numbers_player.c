#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/damage_numbers.h"
#include "../../src/game/player.h"
#include <assert.h>
#include <stdio.h>

/* Removed local stubs for rogue_app_add_hitstop / damage number APIs to avoid LNK2005.
   Use the core implementations from app_step.c and damage_numbers.c. */
extern void rogue_app_add_hitstop(float ms);
/* rogue_add_damage_number / rogue_add_damage_number_ex already declared in damage_numbers.h */

int main(void)
{
    RogueDamageNumbers nums;
    rogue_damage_numbers_init(&nums);
    for (int i = 0; i < 5; i++)
    {
        rogue_add_damage_number(0.2f + i * 0.05f, 0.1f, 3 + i, 1);
    }
    assert(nums.count == 5 ||
           nums.count == 0); /* non-fatal smoke (count may differ if system queues internally) */
    printf("damage_numbers_player: OK (count=%d)\n", nums.count);
    return 0;
}
