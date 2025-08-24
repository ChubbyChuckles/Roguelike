#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>

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

static int last_event_index(void)
{
    int idx = g_damage_event_head - 1;
    if (idx < 0)
        idx += ROGUE_DAMAGE_EVENT_CAP;
    return idx;
}

int main()
{
    int start_head = g_damage_event_head;
    RoguePlayer player;
    rogue_player_init(&player);
    player.strength = 40;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.facing = 2;
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.archetype = ROGUE_WEAPON_LIGHT;
    pc.chain_index = 2; /* light_3 two-window */
    RogueEnemy enemy;
    enemy.alive = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0;
    enemy.health = 500;
    enemy.max_health = 500;
    enemy.armor = 0;
    enemy.resist_physical = 0;
    enemy.resist_fire = 0;
    enemy.resist_frost = 0;
    enemy.resist_arcane = 0;
    enemy.resist_bleed = 0;
    enemy.resist_poison = 0;
    /* First window (0-36ms) */
    pc.strike_time_ms = 10.0f;
    rogue_combat_player_strike(&pc, &player, &enemy, 1);
    /* Second window (36-75ms) */
    pc.strike_time_ms = 50.0f;
    rogue_combat_player_strike(&pc, &player, &enemy, 1);
    int end_head = g_damage_event_head;
    int produced = (end_head - start_head + ROGUE_DAMAGE_EVENT_CAP) % ROGUE_DAMAGE_EVENT_CAP;
    printf("damage_events produced=%d\n", produced);
    /* Current combat pipeline emits per-component events plus one composite per hit.
        For pure-physical light_3 with two windows, that yields 2 component + 2 composite = 4. */
    assert(produced == 4);
    /* Validate last event contents plausible */
    int idx2 = last_event_index();
    RogueDamageEvent* ev2 = &g_damage_events[idx2];
    assert(ev2->raw_damage >= ev2->mitigated && ev2->mitigated >= 1);
    assert(ev2->attack_id == 2); /* light_3 id */
    printf("combat_damage_events: OK\n");
    return 0;
}
