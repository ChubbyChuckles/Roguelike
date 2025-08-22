#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>

/* Stubs */
RoguePlayer g_exposed_player_for_stats = {0};
void rogue_player_init(RoguePlayer* p); /* forward from player.h without including full header */
static int rogue_get_current_attack_frame(void) { return 3; }
static void rogue_app_add_hitstop(float ms) { (void) ms; }
static void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
static void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}

int main(void)
{
    RoguePlayer p;
    rogue_player_init(&p);
    p.base.pos.x = 0;
    p.base.pos.y = 0;
    p.facing = 2;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_LIGHT); /* light_1 has whiff flag per data */
    /* Start attack */
    rogue_combat_update_player(&c, 0.0f, 1); /* press attack */
    /* Advance through windup -> strike (simulate) */
    for (int i = 0; i < 200; i++)
    {
        rogue_combat_update_player(&c, 1.0f, 0);
        if (c.phase == ROGUE_ATTACK_STRIKE)
            break;
    }
    assert(c.phase == ROGUE_ATTACK_STRIKE);
    /* Simulate strike frames with no enemy hit (whiff) and buffer next attack early */
    int transitioned_early = 0;
    for (int t = 0; t < 200; t++)
    {
        if (t == 60)
        { /* around 60ms into strike, buffer next */
            rogue_combat_update_player(&c, 0.0f, 1);
        }
        rogue_combat_update_player(&c, 1.0f, 0);
        if (c.phase == ROGUE_ATTACK_RECOVER)
        {
            transitioned_early = 1;
            break;
        }
    }
    assert(transitioned_early && "Expected early whiff cancel into recover");
    printf("combat_whiff_cancel: OK\n");
    return 0;
}
