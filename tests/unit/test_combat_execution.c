#define SDL_MAIN_HANDLED
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern int g_crit_layering_mode;
extern RogueDamageEvent g_damage_events[];
extern int g_damage_event_head;
extern int g_damage_event_total;

/* Provide required external symbols (link from core) */
extern RoguePlayer g_exposed_player_for_stats;
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

static void reset_events(void) { rogue_damage_events_clear(); }
static int last_index(void)
{
    int idx = g_damage_event_head - 1;
    if (idx < 0)
        idx += ROGUE_DAMAGE_EVENT_CAP;
    return idx;
}

int main()
{
    srand(3333);
    RoguePlayer p;
    rogue_player_init(&p);
    p.facing = 2;
    p.strength = 50;
    p.dexterity = 10;
    p.crit_chance = 0;
    p.crit_damage = 50;
    g_crit_layering_mode = 0;
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.archetype = ROGUE_WEAPON_LIGHT;
    pc.chain_index = 0;
    pc.strike_time_ms = 20.0f; /* inside window */

    /* Case 1: execution by low health percentage (<=15%) */
    RogueEnemy e1;
    e1.alive = 1;
    e1.base.pos.x = 0.7f;
    e1.base.pos.y = 0;
    e1.max_health = 200;
    e1.health = 28; /* 14% */
    e1.armor = 0;
    e1.resist_physical = 0;
    e1.resist_fire = 0;
    e1.resist_frost = 0;
    e1.resist_arcane = 0;
    e1.resist_bleed = 0;
    e1.resist_poison = 0;
    reset_events();
    rogue_combat_player_strike(&pc, &p, &e1, 1);
    int li = last_index();
    RogueDamageEvent* ev = &g_damage_events[li];
    assert(ev->execution == 1 && "expected execution flag for low health kill");

    /* Case 2: execution by overkill percent (>=25%) */
    RogueEnemy e2;
    e2.alive = 1;
    e2.base.pos.x = 0.7f;
    e2.base.pos.y = 0;
    e2.max_health = 300;
    e2.health = 40; /* above 15% => not low health */
    e2.armor = 0;
    e2.resist_physical = 0;
    e2.resist_fire = 0;
    e2.resist_frost = 0;
    e2.resist_arcane = 0;
    e2.resist_bleed = 0;
    e2.resist_poison = 0;
    reset_events();
    /* Make player damage large to ensure overkill >=25% of max (>=75) */
    p.strength = 400; /* large scaling */
    pc.processed_window_mask = 0;
    pc.strike_time_ms = 20.0f;
    rogue_combat_player_strike(&pc, &p, &e2, 1);
    li = last_index();
    ev = &g_damage_events[li];
    assert(ev->execution == 1 && "expected execution flag for overkill kill");

    /* Case 3: non-execution normal kill (health >15%, overkill <25%) */
    RogueEnemy e3;
    e3.alive = 1;
    e3.base.pos.x = 0.7f;
    e3.base.pos.y = 0;
    e3.max_health = 200;
    e3.health = 120;
    e3.armor = 20;
    e3.resist_physical = 20;
    e3.resist_fire = 0;
    e3.resist_frost = 0;
    e3.resist_arcane = 0;
    e3.resist_bleed = 0;
    e3.resist_poison = 0;
    reset_events();
    p.strength = 60;
    pc.processed_window_mask = 0;
    pc.strike_time_ms = 20.0f;
    rogue_combat_player_strike(&pc, &p, &e3, 1);
    li = last_index();
    ev = &g_damage_events[li];
    if (e3.alive == 0)
    {
        assert(ev->execution == 0 && "should not mark execution for standard kill");
    }

    printf("combat_execution: OK\n");
    return 0;
}
