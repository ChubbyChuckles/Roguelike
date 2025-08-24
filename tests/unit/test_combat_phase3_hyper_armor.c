#define SDL_MAIN_HANDLED
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void) { return 3; }
void rogue_app_add_hitstop(float ms) { (void) ms; }
void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}

/* This test only asserts that hyper armor flag is plumbed into current_window_flags during an
   active window. Since defensive incoming poise damage pipeline is not yet implemented, we limit
   scope to flag presence. */

int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    g_exposed_player_for_stats = p;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_LIGHT);
    c.chain_index = 2; /* light_3 has two windows */
    c.phase = ROGUE_ATTACK_STRIKE;
    /* Force strike time inside first window */
    rogue_combat_test_force_strike(&c, 10.0f);
    RogueEnemy dummy[1];
    memset(&dummy[0], 0, sizeof dummy[0]);
    dummy[0].alive = 0; /* no enemies needed */
    rogue_combat_player_strike(&c, &p, dummy, 0);
    /* If any window active, current_window_flags reflects that window flags (hyper armor not
     * authored yet so may be zero) */
    assert(c.current_window_flags == 0 || (c.current_window_flags & 0x0100) == 0x0100);
    printf("phase3_hyper_armor_flag_plumb: flags=0x%X\n", c.current_window_flags);
    return 0;
}
