#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>

/* Stubs */
RoguePlayer g_exposed_player_for_stats = {0};
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

static void advance_until_phase(RoguePlayerCombat* c, RogueAttackPhase ph)
{
    for (int i = 0; i < 400; i++)
    {
        rogue_combat_update_player(c, 1.0f, 0);
        if (c->phase == ph)
            return;
    }
}

int main(void)
{
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_LIGHT); /* light_1 has block cancel */
    /* Start attack */
    rogue_combat_update_player(&c, 0, 1);
    advance_until_phase(&c, ROGUE_ATTACK_STRIKE);
    assert(c.phase == ROGUE_ATTACK_STRIKE);
    /* Simulate some strike time */
    for (int i = 0; i < 25; i++)
        rogue_combat_update_player(&c, 1.0f, 0);
    /* Notify block */
    rogue_combat_notify_blocked(&c);
    /* Provide buffered follow-up */
    rogue_combat_update_player(&c, 0, 1);
    /* Advance a short window - should early cancel into recover due to block */
    for (int i = 0; i < 40; i++)
    {
        rogue_combat_update_player(&c, 1.0f, 0);
        if (c.phase == ROGUE_ATTACK_RECOVER)
            break;
    }
    assert(c.phase == ROGUE_ATTACK_RECOVER && "Expected block cancel into recover");
    printf("combat_block_cancel: OK\n");
    return 0;
}
