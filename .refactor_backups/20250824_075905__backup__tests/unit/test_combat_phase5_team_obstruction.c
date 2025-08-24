#define SDL_MAIN_HANDLED
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* Use combat override hook instead of macro to avoid macro collisions. */
#include "../../src/core/app/app_state.h"
#include "../../src/core/navigation.h"
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat_attacks.h"
/* Provide line obstruction hook via registration */
static int g_obstruction_phase = 0; /* 0 baseline, 1 obstruct */
static int test_line_obstruct(float sx, float sy, float ex, float ey)
{
    (void) sx;
    (void) sy;
    (void) ex;
    (void) ey;
    return g_obstruction_phase ? 1 : 0;
}

/* Minimal overrides: only provide what isn't already in core or needs deterministic behavior. */
RoguePlayer g_exposed_player_for_stats; /* required by combat.c for cc flags */
extern int rogue_force_attack_active;   /* defined in combat.c */
extern int g_attack_frame_override;     /* defined in combat.c */
/* Provide a deterministic blocked tile for obstruction attenuation (tile 3,0). */
static int test_nav_is_blocked(int tx, int ty) { return (tx == 2 && ty == 0) ? 1 : 0; }
/* Use extern declarations for core functions to avoid duplicate definitions. */
extern void rogue_add_damage_number(float x, float y, int amount, int from_player);
extern void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit);
extern int rogue_buffs_get_total(int t);
/* Use core mitigation; no override needed. */
/* Attack table stub: single attack def (reuse real struct from header) */
static const RogueAttackDef g_stub_attack = {
    0,                  /* id */
    "stub",             /* name */
    ROGUE_WEAPON_LIGHT, /* archetype */
    0,                  /* chain_index */
    0.0f,               /* startup_ms */
    80.0f,              /* active_ms */
    0.0f,               /* recovery_ms */
    5.0f,               /* stamina_cost */
    0.0f,               /* poise_damage */
    20.0f,              /* base_damage */
    ROGUE_DMG_PHYSICAL, /* damage_type */
    0.30f,              /* str_scale */
    0.0f,               /* dex_scale */
    0.0f,               /* int_scale */
    1,                  /* num_windows */
    {{0, 80, 0, 1.0f, 0, 0, 0,
      0}}, /* windows
              (start_ms,end_ms,flags,damage_mult,bleed_build,frost_build,start_frame,end_frame) */
    0.0f,  /* poise_cost */
    0,     /* cancel_flags */
    0.50f, /* whiff_cancel_pct */
    0.0f,  /* bleed_build */
    0.0f   /* frost_build */
};
const RogueAttackDef* rogue_attack_get(int arch, int ci)
{
    (void) arch;
    (void) ci;
    return &g_stub_attack;
}
int rogue_attack_chain_length(int arch)
{
    (void) arch;
    return 1;
}

int main()
{
    /* Force active strike frame */
    rogue_force_attack_active = 1;
    g_attack_frame_override = 3;
    RoguePlayerCombat pc;
    rogue_combat_init(&pc);
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.strike_time_ms = 10; /* inside window */
    rogue_combat_set_obstruction_line_test(test_line_obstruct);
    RoguePlayer player;
    memset(&player, 0, sizeof player);
    player.team_id = 0;
    player.strength = 30;
    player.facing = 2;
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    /* Two enemies: ally (same team) and foe; both within reach; ensure ally untouched */
    RogueEnemy enemies[3];
    memset(enemies, 0, sizeof enemies);
    enemies[0].alive = 1;
    enemies[0].team_id = 0;
    enemies[0].base.pos.x = 1.0f;
    enemies[0].base.pos.y = 0;
    enemies[0].health = 100;
    enemies[0].max_health = 100;
    enemies[1].alive = 1;
    enemies[1].team_id = 1;
    enemies[1].base.pos.x = 1.0f;
    enemies[1].base.pos.y = 0;
    enemies[1].health = 100;
    enemies[1].max_health = 100;
    int kills = rogue_combat_player_strike(&pc, &player, enemies, 2);
    (void) kills;
    if (enemies[0].health != 100)
    {
        printf("fail_friend_fire health=%d\n", enemies[0].health);
        return 1;
    }
    if (enemies[1].health == 100)
    {
        printf("fail_enemy_untouched\n");
        return 2;
    }
    int dmg_full = enemies[1].max_health - enemies[1].health;
    /* Obstruction test: place enemy behind blocking tile (2,0). Without obstruction would be in
     * reach. */
    pc.phase = ROGUE_ATTACK_STRIKE;
    pc.strike_time_ms = 10;
    pc.processed_window_mask = 0; /* reset window */
    g_obstruction_phase = 1;      /* enable obstruction for second strike */
    enemies[1].health = enemies[1].max_health;
    enemies[1].base.pos.x = 3.6f; /* still within reach due to center shift; crosses tile (2,0) */
    int kills2 = rogue_combat_player_strike(&pc, &player, enemies, 2);
    (void) kills2;
    int dmg_obstruct = enemies[1].max_health - enemies[1].health;
    int ratio = (dmg_obstruct * 100) / (dmg_full ? dmg_full : 1);
    if (!(dmg_obstruct < dmg_full && ratio >= 50 && ratio <= 60))
    {
        printf("fail_obstruction_scale full=%d obstruct=%d ratio=%d%%\n", dmg_full, dmg_obstruct,
               ratio);
        return 3;
    }
    printf("phase5_team_obstruction: OK full=%d obstruct=%d\n", dmg_full, dmg_obstruct);
    return 0;
}
