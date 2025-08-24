#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "core/app_state.h"
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>

/* Minimal stubs */
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
    if (amount == 0)
        return;
    if (g_app.dmg_number_count < 128)
    {
        int i = g_app.dmg_number_count++;
        g_app.dmg_numbers[i].x = x;
        g_app.dmg_numbers[i].y = y;
        g_app.dmg_numbers[i].amount = amount;
        g_app.dmg_numbers[i].from_player = from_player;
        g_app.dmg_numbers[i].crit = crit;
    }
}
/* use core implementation of rogue_app_damage_number_count (no local duplicate) */

static void reset_app_state(void) { g_app.dmg_number_count = 0; }

int main()
{
    reset_app_state();
    /* Force deterministic non-crit for this test to avoid RNG flakiness */
    extern int g_force_crit_mode;
    g_force_crit_mode = 0;
    RoguePlayer player;
    rogue_player_init(&player);
    player.strength = 30;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.facing = 2;
    RogueEnemy enemy;
    enemy.alive = 1;
    enemy.base.pos.x = 1.2f;
    enemy.base.pos.y = 0;
    enemy.health = 500;
    enemy.max_health = 500;
    RoguePlayerCombat combat;
    rogue_combat_init(&combat);
    combat.phase = ROGUE_ATTACK_STRIKE;
    /* Strike once */
    int before = rogue_app_damage_number_count();
    rogue_combat_player_strike(&combat, &player, &enemy, 1);
    int after = rogue_app_damage_number_count();
    assert(after == before + 1 && "Expected one player damage number spawned");
    assert(g_app.dmg_numbers[after - 1].from_player == 1);
    assert(g_app.dmg_numbers[after - 1].crit == 0);
    printf("damage_numbers_player: OK count=%d amount=%d\n", after,
           g_app.dmg_numbers[after - 1].amount);
    return 0;
}
