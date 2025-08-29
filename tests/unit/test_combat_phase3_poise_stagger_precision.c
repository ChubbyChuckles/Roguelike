#define SDL_MAIN_HANDLED
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

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

/* Precision test: ensure stagger triggers exactly when cumulative poise damage >= poise_max and not
 * before. */
static void setup_enemy(RogueEnemy* e)
{
    memset(e, 0, sizeof *e);
    e->alive = 1;
    e->health = 500;
    e->max_health = 500;
    e->armor = 0;
    e->resist_physical = 0;
    e->poise_max = 40.0f;
    e->poise = e->poise_max;
    e->base.pos.x = 1.0f;
}

int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    p.strength = 30;
    p.dexterity = 5;
    p.intelligence = 5;
    g_exposed_player_for_stats = p;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_HEAVY);
    c.chain_index = 0; /* heavy_1 single window */
    c.phase = ROGUE_ATTACK_STRIKE;
    c.strike_time_ms = 0.0f;
    RogueEnemy e;
    setup_enemy(&e);
    /* We know heavy_1 poise_damage from attack def table: 28 (set in field after stamina_cost).
     * We'll apply twice to cross 40 threshold. */
    /* First hit: expect poise reduced but not stagger (40-28=12) */
    rogue_combat_test_force_strike(&c, 10.0f);
    rogue_combat_player_strike(&c, &p, &e, 1);
    assert(e.poise < e.poise_max && e.poise > 0.0f && e.staggered == 0);
    float after_first = e.poise;
    /* Reset processed mask to allow window to apply again deterministically */
    c.processed_window_mask = 0;
    c.emitted_events_mask = 0;
    c.event_count = 0;
    c.strike_time_ms = 10.0f; /* same window again */
    rogue_combat_player_strike(&c, &p, &e, 1);
    assert(e.poise <= 0.0f && e.staggered == 1);
    printf("phase3_poise_stagger_precision: OK (first=%.2f second=%.2f)\n", after_first, e.poise);
    return 0;
}
