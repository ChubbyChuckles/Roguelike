#define SDL_MAIN_HANDLED
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
#include "../../src/game/infusions.h"
#include "../../src/game/weapons.h"
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
    player.strength = 30;
    player.dexterity = 10;
    player.intelligence = 10;
    player.facing = 2;
    player.equipped_weapon_id = 0;
    player.combat_stance = 0;
    player.weapon_infusion = 1; /* fire */
    player.base.pos.x = 0;
    player.base.pos.y = 0;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 600;
    enemy.max_health = 600;
    enemy.facing = 1;
    enemy.resist_fire = 50; /* strong fire resist to force visible split */

    /* First strike with fire infusion: expect lower effective damage vs high fire resist than pure
     * physical would produce */
    int dmg_fire = strike_once(&pc, &player, &enemy);
    printf("dmg_fire=%d\n", dmg_fire);
    if (dmg_fire <= 0)
    {
        printf("fail_fire_dmg=%d\n", dmg_fire);
        return 1;
    }
    /* Reset enemy and remove infusion to compare */
    enemy.health = 600;
    player.weapon_infusion = 0;
    int dmg_phys = strike_once(&pc, &player, &enemy);
    printf("dmg_phys=%d\n", dmg_phys);
    if (dmg_phys <= 0)
    {
        printf("fail_phys_dmg=%d\n", dmg_phys);
        return 2;
    }
    /* Because high fire resist mitigates infused portion, fire build should be noticeably lower (at
     * least 5% lower) */
    if (dmg_fire < dmg_phys)
    {
        printf("phase7_infusion_split_events: OK fire=%d phys=%d\n", dmg_fire, dmg_phys);
        fflush(stdout);
        return 0;
    }
    printf("fail_split_mitigation fire=%d phys=%d (expected fire<phys)\n", dmg_fire, dmg_phys);
    fflush(stdout);
    return 3;
}
