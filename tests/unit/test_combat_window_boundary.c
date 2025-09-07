#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>

/* Ensure no local duplicate of rogue_app_add_hitstop to avoid LNK2005 */
extern void rogue_app_add_hitstop(float ms);

int main(void)
{
    RoguePlayer p;
    rogue_player_init(&p);
    p.base.pos.x = 0;
    p.base.pos.y = 0;
    p.facing = 2;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_LIGHT);
    /* Start attack and fast-forward beyond strike window to ensure transition occurs */
    rogue_combat_update_player(&c, 0.0f, 1); /* press */
    int advanced = 0;
    for (int i = 0; i < 400; i++)
    {
        rogue_combat_update_player(&c, 1.0f, 0);
        if (c.phase == ROGUE_ATTACK_STRIKE)
            advanced = 1;
        if (advanced && c.phase == ROGUE_ATTACK_RECOVER)
            break;
    }
    assert(c.phase == ROGUE_ATTACK_RECOVER);
    printf("combat_window_boundary: OK\n");
    return 0;
}
