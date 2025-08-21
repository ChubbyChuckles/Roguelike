#define SDL_MAIN_HANDLED
#include "entities/enemy.h"
#include "entities/player.h"
#include "game/combat.h"
#include "game/infusions.h"
#include "game/weapons.h"
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
                                             70,
                                             0,
                                             5,
                                             0,
                                             20,
                                             ROGUE_DMG_PHYSICAL,
                                             0.0f,
                                             0,
                                             0,
                                             1,
                                             {{0, 70, ROGUE_CANCEL_ON_HIT, 1.0f, 0, 0, 0, 0}},
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

static int strike_once(RoguePlayerCombat* pc, RoguePlayer* pl, RogueEnemy* e)
{
    pc->phase = ROGUE_ATTACK_STRIKE;
    pc->strike_time_ms = 10;
    pc->processed_window_mask = 0;
    pc->emitted_events_mask = 0;
    int hb = e->health;
    rogue_combat_player_strike(pc, pl, e, 1);
    return hb - e->health;
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
    player.dexterity = 20;
    player.intelligence = 25;
    player.facing = 2;
    player.equipped_weapon_id = 3; /* focus catalyst int-scaling */
    player.combat_stance = 0;
    player.weapon_infusion = 3; /* arcane infusion */
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 800;
    enemy.max_health = 800;
    enemy.facing = 1;

    int dmg_arcane = strike_once(&pc, &player, &enemy);
    if (dmg_arcane <= 0)
    {
        printf("fail_arcane_base=%d\n", dmg_arcane);
        return 1;
    }

    enemy.health = 800;
    player.weapon_infusion = 1; /* fire */
    int dmg_fire = strike_once(&pc, &player, &enemy);
    if (!(dmg_fire > 0 && dmg_fire != dmg_arcane))
    {
        printf("fail_fire_variation arc=%d fire=%d\n", dmg_arcane, dmg_fire);
        return 2;
    }

    /* Exhaust durability to trigger reduction */
    for (int i = 0; i < 100; i++)
    {
        enemy.health = 800;
        strike_once(&pc, &player, &enemy);
        enemy.health = 800;
    }
    enemy.health = 800;
    int dmg_low_dur = strike_once(&pc, &player, &enemy);
    if (!(dmg_low_dur < dmg_arcane))
    {
        printf("fail_durability_scalar orig=%d low=%d\n", dmg_arcane, dmg_low_dur);
        return 3;
    }

    printf("phase7_infusions_durability: OK arc=%d fire=%d low_dur=%d\n", dmg_arcane, dmg_fire,
           dmg_low_dur);
    return 0;
}
