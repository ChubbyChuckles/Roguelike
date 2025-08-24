#define SDL_MAIN_HANDLED
#include "../../src/game/combat.h"
#include "../../src/game/lock_on.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Include navigation (not strictly needed) and use combat override hook */
#include "../../src/core/navigation.h"
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat_attacks.h"
static int g_obstruction_phase = 0;
static int test_line_obstruct(float sx, float sy, float ex, float ey)
{
    (void) sx;
    (void) sy;
    (void) ex;
    (void) ey;
    return g_obstruction_phase ? 1 : 0;
}

/* Block tile (2,0) to induce obstruction scaling */
static int test_nav_is_blocked(int tx, int ty) { return (tx == 2 && ty == 0) ? 1 : 0; }

/* Minimal attack stub */
/* Match working stub parameters from team obstruction test for consistent damage */
static const RogueAttackDef g_stub_attack = {0,
                                             "stub",
                                             ROGUE_WEAPON_LIGHT,
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
static const RogueAttackDef* rogue_attack_get(int a, int c)
{
    (void) a;
    (void) c;
    return &g_stub_attack;
}
static int rogue_attack_chain_length(int a)
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

static int strike_once(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy* enemies,
                       int enemy_count)
{
    pc->phase = ROGUE_ATTACK_STRIKE;
    pc->strike_time_ms = 10;
    pc->processed_window_mask = 0;
    pc->emitted_events_mask = 0; /* reset */
    return rogue_combat_player_strike(pc, player, enemies, enemy_count);
}

int main()
{
    rogue_force_attack_active = 1;
    g_attack_frame_override = 3; /* align with active frame approach used elsewhere */
    RoguePlayer player;
    memset(&player, 0, sizeof player);
    player.team_id = 0;
    player.facing = 2;
    player.strength = 40;
    player.lock_on_radius = 12.0f;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    rogue_combat_set_obstruction_line_test(test_line_obstruct);

    /* ENEMIES for latency + cycling */
    RogueEnemy enemies[6];
    memset(enemies, 0, sizeof enemies);
    /* Place three enemies roughly around player (east, south, west) */
    enemies[0].alive = 1;
    enemies[0].base.pos.x = 1.2f;
    enemies[0].base.pos.y = 0.0f;
    enemies[0].health = 200;
    enemies[0].max_health = 200;
    enemies[1].alive = 1;
    enemies[1].base.pos.x = 0.0f;
    enemies[1].base.pos.y = 2.5f;
    enemies[1].health = 200;
    enemies[1].max_health = 200;
    enemies[2].alive = 1;
    enemies[2].base.pos.x = -1.5f;
    enemies[2].base.pos.y = 0.0f;
    enemies[2].health = 200;
    enemies[2].max_health = 200;

    rogue_lockon_reset(&player);
    if (!rogue_lockon_acquire(&player, enemies, 3))
    {
        printf("fail_acquire\n");
        return 1;
    }
    printf("acquired idx=%d\n", player.lock_on_target_index);
    int initial = player.lock_on_target_index;
    if (!rogue_lockon_cycle(&player, enemies, 3, 1))
    {
        printf("fail_cycle_first\n");
        return 2;
    }
    printf("cycled first idx=%d\n", player.lock_on_target_index);
    int after_first = player.lock_on_target_index;
    /* Immediate second cycle should be blocked by cooldown */
    if (rogue_lockon_cycle(&player, enemies, 3, 1))
    {
        printf("fail_cooldown_block_immediate\n");
        return 3;
    }
    printf("cooldown immediate block OK idx=%d\n", player.lock_on_target_index);
    if (player.lock_on_target_index != after_first)
    {
        printf("fail_target_changed_on_block\n");
        return 4;
    }
    /* Partial tick (< cooldown) */
    rogue_lockon_tick(&player, 100.0f);
    if (rogue_lockon_cycle(&player, enemies, 3, 1))
    {
        printf("fail_cooldown_block_partial\n");
        return 5;
    }
    printf("cooldown partial block OK\n");
    /* Complete remaining tick */
    rogue_lockon_tick(&player, 90.0f);
    if (!rogue_lockon_cycle(&player, enemies, 3, 1))
    {
        printf("fail_cycle_after_cooldown\n");
        return 6;
    }
    printf("cycled after cooldown idx=%d\n", player.lock_on_target_index);

    /* Obstruction damping with lock-on: baseline unobstructed damage vs obstructed path */
    RogueEnemy target;
    memset(&target, 0, sizeof target);
    target.alive = 1;
    target.team_id = 1;
    target.base.pos.x = 1.0f;
    target.base.pos.y = 0.0f;
    target.health = 100;
    target.max_health = 100;
    RogueEnemy list[1];
    list[0] = target;
    rogue_lockon_reset(&player);
    if (!rogue_lockon_acquire(&player, list, 1))
    {
        printf("fail_acquire_single\n");
        return 7;
    }
    {
        float dx, dy;
        rogue_lockon_get_dir(&player, list, 1, &dx, &dy);
        player.facing = (dx > 0) ? 2 : (dx < 0 ? 6 : player.facing);
        strike_once(&pc, &player, list, 1);
        int dmg_full = list[0].max_health - list[0].health;
        printf("dmg_full=%d health=%d\n", dmg_full, list[0].health);
        if (dmg_full <= 0)
        {
            printf("fail_no_baseline_damage\n");
            return 9;
        }
        /* Reset health and move behind blocking tile (line crosses tile 2,0 at x=2..3) */
        list[0].health = list[0].max_health;
        list[0].base.pos.x = 3.6f;
        g_obstruction_phase = 1; /* enable obstruction for second strike */
        float dx2, dy2;
        rogue_lockon_get_dir(&player, list, 1, &dx2, &dy2);
        player.facing = (dx2 > 0) ? 2 : (dx2 < 0 ? 6 : player.facing);
        strike_once(&pc, &player, list, 1);
        int dmg_obstruct = list[0].max_health - list[0].health;
        printf("dmg_obstruct=%d health=%d\n", dmg_obstruct, list[0].health);
        int ratio = (dmg_obstruct * 100) / (dmg_full ? dmg_full : 1);
        if (!(dmg_obstruct < dmg_full && ratio >= 50 && ratio <= 60))
        {
            printf("fail_obstruction_lockon full=%d obstruct=%d ratio=%d%%\n", dmg_full,
                   dmg_obstruct, ratio);
            return 8;
        }
        printf("phase5_lock_on_latency_obstruction: OK initial=%d first=%d final=%d full=%d "
               "obstruct=%d ratio=%d%%\n",
               initial, after_first, player.lock_on_target_index, dmg_full, dmg_obstruct, ratio);
    }
    return 0;
}
