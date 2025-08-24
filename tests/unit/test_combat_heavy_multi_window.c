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

static void force_window(RoguePlayerCombat* pc, float t) { pc->strike_time_ms = t; }

int main()
{
    RoguePlayerCombat combat;
    rogue_combat_init(&combat);
    RoguePlayer player;
    rogue_player_init(&player);
    player.strength = 60;
    player.dexterity = 10;
    player.intelligence = 5;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.facing = 2;
    RogueEnemy enemy;
    enemy.alive = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0;
    enemy.health = 1000;
    enemy.max_health = 1000;
    /* heavy_2 has three windows (0-40,40-80,80-105) */
    combat.archetype = ROGUE_WEAPON_HEAVY;
    combat.chain_index = 1;
    combat.phase = ROGUE_ATTACK_STRIKE;
    combat.processed_window_mask = 0;
    combat.emitted_events_mask = 0;
    combat.event_count = 0;
    const RogueAttackDef* def = rogue_attack_get(combat.archetype, combat.chain_index);
    assert(def && def->num_windows == 3);

    int hp0 = enemy.health;
    force_window(&combat, 10.0f);
    rogue_combat_player_strike(&combat, &player, &enemy, 1);
    int after_w0 = enemy.health;
    assert(after_w0 < hp0);
    force_window(&combat, 50.0f);
    rogue_combat_player_strike(&combat, &player, &enemy, 1);
    int after_w1 = enemy.health;
    assert(after_w1 < after_w0);
    force_window(&combat, 90.0f);
    rogue_combat_player_strike(&combat, &player, &enemy, 1);
    int after_w2 = enemy.health;
    assert(after_w2 < after_w1);
    /* Re-enter a processed window (should not apply) */
    int prev = enemy.health;
    force_window(&combat, 90.0f);
    rogue_combat_player_strike(&combat, &player, &enemy, 1);
    assert(enemy.health == prev);

    /* Validate events: BEGIN/END for each window = 6 total by end */
    assert(combat.event_count == 6);
    int begins = 0, ends = 0;
    for (int i = 0; i < combat.event_count; i++)
    {
        if (combat.events[i].type == ROGUE_COMBAT_EVENT_BEGIN_WINDOW)
            begins++;
        else if (combat.events[i].type == ROGUE_COMBAT_EVENT_END_WINDOW)
            ends++;
    }
    assert(begins == 3 && ends == 3);

    printf("combat_heavy_multi_window: OK dmg_seq=(%d,%d,%d) events=%d\n", hp0 - after_w0,
           after_w0 - after_w1, after_w1 - after_w2, combat.event_count);
    return 0;
}
