#define SDL_MAIN_HANDLED
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern int rogue_force_attack_active;
extern int g_attack_frame_override;
static int rogue_get_current_attack_frame(void) { return 3; }
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
static const RogueAttackDef g_test_attack = {0,
                                             "light",
                                             ROGUE_WEAPON_LIGHT,
                                             0,
                                             0,
                                             60,
                                             0,
                                             5,
                                             0,
                                             15,
                                             ROGUE_DMG_PHYSICAL,
                                             0.0f,
                                             0,
                                             0,
                                             1,
                                             {{0, 60, ROGUE_CANCEL_ON_HIT, 1.0f, 0, 0, 0, 0}},
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
    player.poise = 30;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 200;
    enemy.max_health = 200;
    enemy.facing = 1;

    /* Guard break scaffold: set and consume */
    rogue_player_set_guard_break(&player, &pc);
    if (!pc.riposte_ready)
    {
        printf("fail_guard_break_flag\n");
        return 1;
    }
    if (!rogue_player_consume_guard_break_bonus(&pc))
    {
        printf("fail_guard_break_consume\n");
        return 2;
    }

    /* Parry then attempt projectile deflect */
    rogue_player_begin_parry(&player, &pc);
    if (!rogue_player_is_parry_active(&pc))
    {
        printf("fail_parry_active_for_deflect\n");
        return 3;
    }
    float rx = 0, ry = 0;
    int defl = rogue_player_try_deflect_projectile(&player, &pc, -1, 0, &rx, &ry);
    if (!defl)
    {
        printf("fail_deflect_basic\n");
        return 4;
    }
    if (fabsf(rx - 1.0f) > 0.01f || fabsf(ry) > 0.01f)
    {
        printf("fail_deflect_dir rx=%.2f ry=%.2f\n", rx, ry);
        return 5;
    }

    /* Aerial placeholder: set airborne flag -> next strike consumes landing lag extension (simulate
     * by setting landing lag manually) */
    pc.landing_lag_ms = 100.0f;
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.strike_time_ms = 59.0f; /* near end */
    /* Trigger end -> should transfer landing lag into recovery extension (timer adjust) */
    rogue_combat_player_strike(
        &pc, &player, &enemy,
        1); /* strike processes, move to recover next update when update called */
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.strike_time_ms = 60.0f; /* force threshold */
    rogue_combat_update_player(
        &pc, 1.0f, 0); /* should transition and apply landing lag (precise_accum negative) */

    fprintf(stderr, "phase6_aerial_deflect_guardbreak: OK\n");
    fflush(stderr);
    return 0;
}
