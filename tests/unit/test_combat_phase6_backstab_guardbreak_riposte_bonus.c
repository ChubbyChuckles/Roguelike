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
    int h_before = enemy->health;
    rogue_combat_player_strike(pc, player, enemy, 1);
    return h_before - enemy->health;
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
    player.strength = 50;
    player.facing = 2;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    player.poise_max = 50;
    player.poise = 30;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 300;
    enemy.max_health = 300;
    enemy.facing = 1; /* facing left */

    /* Baseline damage */
    int base_dmg = do_strike(&pc, &player, &enemy);
    if (base_dmg <= 0)
    {
        printf("fail_baseline base=%d\n", base_dmg);
        return 1;
    }

    /* Reset enemy and test backstab multiplier */
    enemy.health = 300;
    player.base.pos.x = enemy.base.pos.x + 0.8f;
    player.base.pos.y = enemy.base.pos.y; /* behind enemy (enemy facing left) */
    if (!rogue_combat_try_backstab(&player, &pc, &enemy))
    {
        printf("fail_backstab_detect\n");
        return 2;
    }
    /* Move player to front again for consistent strike geometry */
    player.base.pos.x = 0.2f;
    player.base.pos.y = 0.0f;
    int bs_dmg = do_strike(&pc, &player, &enemy);
    if (!(bs_dmg > (int) (base_dmg * 1.60f)))
    {
        printf("fail_backstab_mult base=%d backstab=%d\n", base_dmg, bs_dmg);
        return 3;
    }

    /* Riposte multiplier test */
    enemy.health = 300;
    rogue_player_begin_parry(&player, &pc);
    if (!rogue_player_register_incoming_attack_parry(&player, &pc, 1, 0))
    {
        printf("fail_parry\n");
        return 4;
    }
    if (!rogue_player_try_riposte(&player, &pc, &enemy))
    {
        printf("fail_riposte_consume\n");
        return 5;
    }
    int rip_dmg = do_strike(&pc, &player, &enemy);
    if (!(rip_dmg > (int) (base_dmg * 2.1f)))
    {
        printf("fail_riposte_mult base=%d rip=%d\n", base_dmg, rip_dmg);
        return 6;
    }

    /* Guard-break guaranteed crit + damage bonus */
    enemy.health = 300;
    rogue_player_set_guard_break(&player, &pc);
    if (!pc.guard_break_ready)
    {
        printf("fail_guard_break_flag\n");
        return 7;
    }
    if (!rogue_player_consume_guard_break_bonus(&pc))
    {
        printf("fail_guard_break_consume\n");
        return 8;
    }
    int gb_dmg = do_strike(&pc, &player, &enemy);
    if (!(gb_dmg > (int) (base_dmg * 1.40f)))
    {
        printf("fail_guard_break_mult base=%d gb=%d\n", base_dmg, gb_dmg);
        return 9;
    }

    printf("phase6_backstab_guardbreak_riposte_bonus: OK base=%d backstab=%d riposte=%d "
           "guardbreak=%d\n",
           base_dmg, bs_dmg, rip_dmg, gb_dmg);
    return 0;
}
