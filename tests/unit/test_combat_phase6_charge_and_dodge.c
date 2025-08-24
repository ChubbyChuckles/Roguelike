#define SDL_MAIN_HANDLED
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include "../../src/game/combat_attacks.h"
#include "../../src/game/lock_on.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern int rogue_force_attack_active;
extern int g_attack_frame_override;
static int rogue_get_current_attack_frame(void) { return 3; }
RoguePlayer g_exposed_player_for_stats = {0};
static int rogue_buffs_get_total(int t)
{
    (void) t;
    return 0;
}
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
/* Minimal single-window attack def for deterministic output */
static const RogueAttackDef g_test_attack = {0,
                                             "light",
                                             ROGUE_WEAPON_LIGHT,
                                             0,
                                             0,
                                             80,
                                             0,
                                             5,
                                             0,
                                             20,
                                             ROGUE_DMG_PHYSICAL,
                                             0.0f,
                                             0,
                                             0,
                                             1,
                                             {{0, 80, ROGUE_CANCEL_ON_HIT, 1.0f, 0, 0, 0, 0}},
                                             0,
                                             ROGUE_CANCEL_ON_HIT,
                                             0.40f,
                                             0,
                                             0};
const RogueAttackDef* rogue_attack_get(int a, int b)
{
    (void) a;
    (void) b;
    return &g_test_attack;
}
int rogue_attack_chain_length(int a)
{
    (void) a;
    return 1;
}

static int do_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy* enemy)
{
    pc->phase = ROGUE_ATTACK_STRIKE;
    pc->strike_time_ms = 10;
    pc->processed_window_mask = 0;
    pc->emitted_events_mask = 0;
    return rogue_combat_player_strike(pc, player, enemy, 1);
}

int main()
{
    rogue_force_attack_active = 1;
    g_attack_frame_override = 3;
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    RoguePlayer player;
    memset(&player, 0, sizeof player);
    player.team_id = 0;
    player.strength = 40;
    player.facing = 2;
    player.poise_max = 50;
    player.poise = 25;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.lock_on_radius = 5.0f;
    g_exposed_player_for_stats = player;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 200;
    enemy.max_health = 200;

    /* Baseline strike (no charge) */
    int k = do_strike(&pc, &player, &enemy);
    (void) k;
    int dmg_base = 200 - enemy.health;
    if (dmg_base <= 0)
    {
        printf("fail_baseline_damage=%d\n", dmg_base);
        return 1;
    }

    /* Reset enemy and perform charged attack (simulate hold 800ms) */
    enemy.health = 200;
    pc.phase = ROGUE_ATTACK_IDLE;
    pc.pending_charge_damage_mult = 1.0f;
    rogue_combat_charge_begin(&pc);
    for (int t = 0; t < 8; t++)
    {
        rogue_combat_charge_tick(&pc, 100, 1);
    }
    /* release */
    rogue_combat_charge_tick(&pc, 0, 0);
    int k2 = do_strike(&pc, &player, &enemy);
    (void) k2;
    int dmg_charged = 200 - enemy.health;
    if (!(dmg_charged > dmg_base * 2 - 2))
    {
        printf("fail_charged dmg_base=%d charged=%d\n", dmg_base, dmg_charged);
        return 2;
    }

    /* Ensure idle state before dodge (strike test left phase STRIKE); reset to idle */
    pc.phase = ROGUE_ATTACK_IDLE;
    pc.timer = 0;
    pc.stamina = 100.0f; /* full stamina */
    /* Test dodge roll: sufficient stamina & grants i-frames */
    float pre_stam = pc.stamina;
    if (!rogue_player_dodge_roll(&player, &pc, 1))
    {
        printf("fail_dodge_initiate\n");
        return 3;
    }
    if (!(player.iframes_ms >= 399.0f && pc.stamina < pre_stam))
    {
        printf("fail_dodge_effect iframes=%.2f stam=%.2f pre=%.2f\n", player.iframes_ms, pc.stamina,
               pre_stam);
        return 4;
    }
    /* Attempt second immediate dodge with insufficient stamina (drain manually) */
    pc.stamina = 5.0f;
    if (rogue_player_dodge_roll(&player, &pc, 2))
    {
        printf("fail_dodge_should_fail_low_stam\n");
        return 5;
    }

    printf("phase6_charge_and_dodge: OK base=%d charged=%d iframes=%.1f\n", dmg_base, dmg_charged,
           player.iframes_ms);
    return 0;
}
