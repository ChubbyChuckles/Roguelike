#define SDL_MAIN_HANDLED
#include "../../src/entities/player.h"
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

/* Minimal hyper armor simulation: manually toggle active state before incoming hit and ensure poise
 * doesn't drop. */
int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    g_exposed_player_for_stats = p;
    p.poise = p.poise_max;
    p.facing = 0; /* down */
    rogue_player_set_hyper_armor_active(1);
    float before = p.poise;
    int blocked = 0, perfect = 0;
    int applied = rogue_player_apply_incoming_melee(&p, 50.0f, 0.0f, 1.0f, 25, &blocked, &perfect);
    assert(applied == 0 ||
           applied ==
               50); /* either perfect guard zero (if guard active) or damage; poise unaffected */
    assert(p.poise == before); /* poise unchanged due to hyper armor */
    rogue_player_set_hyper_armor_active(0);
    before = p.poise;
    applied = rogue_player_apply_incoming_melee(&p, 50.0f, 0.0f, 1.0f, 25, &blocked, &perfect);
    /* Without hyper armor and not guarding -> poise reduced */
    assert(p.poise < before || p.poise == 0.0f);
    printf("phase3_hyper_armor_immunity: OK\n");
    return 0;
}
