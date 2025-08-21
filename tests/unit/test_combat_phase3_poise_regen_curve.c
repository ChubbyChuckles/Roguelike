#define SDL_MAIN_HANDLED
#include "entities/player.h"
#include "game/combat.h"
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

int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    g_exposed_player_for_stats = p;
    /* Drain poise significantly and set delay */
    p.poise = p.poise_max * 0.10f; /* very low */
    p.poise_regen_delay_ms = 0.0f; /* allow regen */
    float before = p.poise;
    rogue_player_poise_regen_tick(&p, 100.0f); /* 100 ms */
    float after_low = p.poise;
    /* Reset higher poise and compare regen amount to ensure early curve is stronger (quadratic) */
    p.poise = p.poise_max * 0.75f;
    float before_high = p.poise;
    rogue_player_poise_regen_tick(&p, 100.0f);
    float after_high = p.poise;
    float gain_low = after_low - before;
    float gain_high = after_high - before_high;
    assert(gain_low > gain_high); /* early missing % should regen faster */
    printf("phase3_poise_regen_curve: OK (low_gain=%.4f high_gain=%.4f)\n", gain_low, gain_high);
    return 0;
}
