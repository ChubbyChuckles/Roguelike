#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "game/combat.h"
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

static void simulate_to_strike(RoguePlayerCombat* c, float windup_ms)
{
    /* Start attack */
    rogue_combat_update_player(c, 0, 1);
    float acc = 0;
    while (acc < windup_ms + 1)
    {
        float step = 5.0f;
        if (acc + step > windup_ms + 1)
            step = (windup_ms + 1) - acc;
        rogue_combat_update_player(c, step, 0);
        acc += step;
    }
}

int main()
{
    RoguePlayerCombat combat;
    rogue_combat_init(&combat);
    RoguePlayer player;
    rogue_player_init(&player);
    player.strength = 20;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.facing = 2;
    RogueEnemy enemy;
    enemy.alive = 0; /* no enemy so windows just emit begin/end */
    /* Use light chain index 2 (light_3) multi-window */
    combat.chain_index = 2;
    combat.archetype = ROGUE_WEAPON_LIGHT; /* fast path: directly strike */
    combat.phase = ROGUE_ATTACK_STRIKE;
    combat.strike_time_ms = 0;
    combat.processed_window_mask = 0;
    combat.event_count = 0;
    combat.emitted_events_mask = 0;
    const RogueAttackDef* def = rogue_attack_get(combat.archetype, combat.chain_index);
    assert(def && def->num_windows == 2);
    /* First window active at t=10 */
    combat.strike_time_ms = 10.0f;
    rogue_combat_player_strike(&combat, &player, &enemy, 0);
    /* Second window active at t=50 */
    combat.strike_time_ms = 50.0f;
    rogue_combat_player_strike(&combat, &player, &enemy, 0);
    /* Expect 4 events: begin0,end0,begin1,end1 (order may interleave but size==4) */
    assert(combat.event_count == 4);
    int begins = 0, ends = 0;
    for (int i = 0; i < combat.event_count; i++)
    {
        if (combat.events[i].type == ROGUE_COMBAT_EVENT_BEGIN_WINDOW)
            begins++;
        else if (combat.events[i].type == ROGUE_COMBAT_EVENT_END_WINDOW)
            ends++;
    }
    assert(begins == 2 && ends == 2);
    printf("combat_events: OK events=%d begin=%d end=%d\n", combat.event_count, begins, ends);
    return 0;
}
