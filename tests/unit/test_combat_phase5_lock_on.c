#define SDL_MAIN_HANDLED
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include "../../src/game/combat_attacks.h"
#include "../../src/game/lock_on.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Minimal attack stub (reuse struct) */
static const RogueAttackDef g_stub_attack = {0,
                                             "stub",
                                             0,
                                             0,
                                             0,
                                             80,
                                             0,
                                             5,
                                             0,
                                             20,
                                             ROGUE_DMG_PHYSICAL,
                                             0.30f,
                                             0,
                                             0,
                                             1,
                                             {{0, 80, 0, 1.0f, 0, 0, 0, 0}},
                                             0,
                                             0,
                                             0.50f,
                                             0,
                                             0};
const RogueAttackDef* rogue_attack_get(int a, int c)
{
    (void) a;
    (void) c;
    return &g_stub_attack;
}
int rogue_attack_chain_length(int a)
{
    (void) a;
    return 1;
}
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
extern int rogue_force_attack_active;
extern int g_attack_frame_override;
static int rogue_get_current_attack_frame(void) { return 3; }

int main()
{
    RoguePlayer player;
    memset(&player, 0, sizeof player);
    player.facing = 2;
    player.strength = 25;
    player.lock_on_radius = 10.0f;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    rogue_force_attack_active = 0; /* ensure combat uses gating path */
    g_attack_frame_override = 3;   /* active frame */
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.strike_time_ms = 10;
    /* Three enemies positioned in different directions */
    RogueEnemy enemies[4];
    memset(enemies, 0, sizeof enemies);
    enemies[0].alive = 1;
    enemies[0].base.pos.x = 1.2f;
    enemies[0].base.pos.y = 0.0f;
    enemies[0].health = 100;
    enemies[0].max_health = 100; /* east (closer to ensure hit) */
    enemies[1].alive = 1;
    enemies[1].base.pos.x = 0.0f;
    enemies[1].base.pos.y = 3.2f;
    enemies[1].health = 100;
    enemies[1].max_health = 100; /* south */
    enemies[2].alive = 1;
    enemies[2].base.pos.x = -2.5f;
    enemies[2].base.pos.y = 0.0f;
    enemies[2].health = 100;
    enemies[2].max_health = 100; /* west */

    rogue_lockon_reset(&player);
    printf("lock_on_test_start radius=%.2f\n", player.lock_on_radius);
    fflush(stdout);
    if (!rogue_lockon_acquire(&player, enemies, 3))
    {
        printf("fail_acquire n=3 radius=%.2f\n", player.lock_on_radius);
        fflush(stdout);
        return 1;
    }
    printf("acquire_ok target=%d active=%d\n", player.lock_on_target_index, player.lock_on_active);
    fflush(stdout);
    if (!player.lock_on_active || player.lock_on_target_index < 0)
    {
        printf("fail_active_flag idx=%d act=%d\n", player.lock_on_target_index,
               player.lock_on_active);
        fflush(stdout);
        return 2;
    }
    int first_target = player.lock_on_target_index;
    /* Cycle forward */
    printf("cycle_forward_attempt from=%d\n", first_target);
    fflush(stdout);
    if (!rogue_lockon_cycle(&player, enemies, 3, 1))
    {
        printf("fail_cycle_forward\n");
        fflush(stdout);
        return 3;
    }
    printf("cycle_forward_ok new=%d\n", player.lock_on_target_index);
    fflush(stdout);
    if (player.lock_on_target_index == first_target)
    {
        printf("fail_cycle_no_change\n");
        fflush(stdout);
        return 4;
    }
    /* Simulate cooldown tick then cycle backward */
    rogue_lockon_tick(&player, 200.0f);
    printf("tick_done cooldown=%.2f after_forward=%d\n", player.lock_on_switch_cooldown_ms,
           player.lock_on_target_index);
    fflush(stdout);
    int after_forward = player.lock_on_target_index;
    printf("cycle_backward_attempt from=%d\n", after_forward);
    fflush(stdout);
    if (!rogue_lockon_cycle(&player, enemies, 3, -1))
    {
        printf("fail_cycle_backward\n");
        fflush(stdout);
        return 5;
    }
    printf("cycle_backward_ok new=%d expected_first=%d\n", player.lock_on_target_index,
           first_target);
    fflush(stdout);
    if (player.lock_on_target_index != first_target)
    {
        printf("fail_cycle_back_not_original\n");
        fflush(stdout);
        return 6;
    }
    /* Damage direction assist: lock stays on east target so strike should damage that enemy
     * preferentially */
    player.lock_on_target_index = 0;
    player.lock_on_active = 1;
    enemies[0].health = 100;
    enemies[1].health = 100;
    enemies[2].health = 100;
    /* Increase strength to enlarge reach to ensure hit on closer target */
    player.strength = 100;
    float ldx = 0, ldy = 0;
    printf("dir_fetch_attempt target=%d\n", player.lock_on_target_index);
    fflush(stdout);
    if (!rogue_lockon_get_dir(&player, enemies, 3, &ldx, &ldy))
    {
        printf("fail_directional_dir\n");
        fflush(stdout);
        return 7;
    }
    printf("dir_fetched dx=%.3f dy=%.3f\n", ldx, ldy);
    fflush(stdout);
    if (ldx < 0.5f)
    {
        printf("fail_direction_vector dx=%.3f dy=%.3f\n", ldx, ldy);
        fflush(stdout);
        return 7;
    }
    /* Invalidate target (set dead) and validate clearing */
    printf("invalidate_target idx0 alive->0\n");
    fflush(stdout);
    enemies[0].alive = 0;
    rogue_lockon_validate(&player, enemies, 3);
    printf("post_validate active=%d idx=%d\n", player.lock_on_active, player.lock_on_target_index);
    fflush(stdout);
    if (player.lock_on_active)
    {
        printf("fail_validate_clear\n");
        fflush(stdout);
        return 8;
    }
    printf(
        "phase5_lock_on: OK cycle and direction success first=%d fwd=%d back=%d dx=%.2f dy=%.2f\n",
        first_target, after_forward, player.lock_on_target_index, ldx, ldy);
    fflush(stdout);
    return 0;
}
