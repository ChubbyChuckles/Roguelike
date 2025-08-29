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

static void setup_basic_enemy(RogueEnemy* e)
{
    memset(e, 0, sizeof *e);
    e->alive = 1;
    e->base.pos.x = 1.0f;
    e->base.pos.y = 0.0f;
    e->health = 1000;
    e->max_health = 1000;
    e->armor = 0;
    e->resist_physical = 0;
    e->poise_max = 30.0f;
    e->poise = e->poise_max;
}

int main()
{
    RoguePlayer p;
    rogue_player_init(&p);
    p.strength = 25;
    p.dexterity = 5;
    p.intelligence = 5;
    g_exposed_player_for_stats = p;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_HEAVY);
    c.chain_index = 1; /* heavy_2 with multi windows */
    c.phase = ROGUE_ATTACK_STRIKE;
    c.strike_time_ms = 0.0f;
    c.processed_window_mask = 0;
    c.emitted_events_mask = 0;
    c.event_count = 0;
    RogueEnemy enemies[1];
    setup_basic_enemy(&enemies[0]);
    /* Apply successive strike window evaluations to drain poise */
    for (int step = 0; step < 6 && !enemies[0].staggered; ++step)
    {
        rogue_combat_test_force_strike(&c, (float) (step * 20)); /* advance strike time */
        rogue_combat_player_strike(&c, &p, enemies, 1);
    }
    assert(enemies[0].poise <= 0.0f && enemies[0].staggered == 1);
    /* Simulate stagger decay */
    enemies[0].stagger_timer_ms = 50.0f;
    enemies[0].staggered = 1;
    /* Fake a minimal enemy_system like regen tick: directly mimic logic */
    for (int i = 0; i < 20; i++)
    {
        if (enemies[0].staggered)
        {
            enemies[0].stagger_timer_ms -= 16.0f;
            if (enemies[0].stagger_timer_ms <= 0)
            {
                enemies[0].staggered = 0;
                enemies[0].poise = enemies[0].poise_max * 0.50f;
            }
        }
    }
    assert(enemies[0].staggered == 0 && enemies[0].poise > 0.0f);
    printf("phase3_poise_stagger: OK (poise=%.2f)\n", enemies[0].poise);
    return 0;
}
