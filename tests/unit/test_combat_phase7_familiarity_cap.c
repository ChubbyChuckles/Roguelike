#define SDL_MAIN_HANDLED
#include "../../src/entities/enemy.h"
#include "../../src/entities/player.h"
#include "../../src/game/combat.h"
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

static const RogueAttackDef g_attack = {0,
                                        "fam_test",
                                        ROGUE_WEAPON_LIGHT,
                                        0,
                                        0,
                                        60,
                                        0,
                                        8,
                                        0,
                                        20,
                                        ROGUE_DMG_PHYSICAL,
                                        0.5f,
                                        0.0f,
                                        0.0f,
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
    return &g_attack;
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

RoguePlayer g_exposed_player_for_stats = {0};

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
    player.dexterity = 10;
    player.intelligence = 5;
    player.facing = 2;
    player.equipped_weapon_id = 0;
    g_exposed_player_for_stats = player;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 5000;
    enemy.max_health = 5000;
    enemy.facing = 1;

    int base = strike_once(&pc, &player, &enemy);
    enemy.health = 5000;
    /* Push usage well beyond cap */
    for (int i = 0; i < 400; i++)
    {
        strike_once(&pc, &player, &enemy);
        enemy.health = 5000;
    }
    int capped = strike_once(&pc, &player, &enemy);
    if (!(capped > base))
    {
        printf("fail_familiarity_not_increasing base=%d capped=%d\n", base, capped);
        return 1;
    }
    /* Approx bonus should not exceed 10% of (base weapon component + stats). Allow slight float
     * rounding. */
    if (capped > (int) (base * 1.11f))
    {
        printf("fail_familiarity_cap_exceeded base=%d capped=%d\n", base, capped);
        return 2;
    }
    printf("phase7_familiarity_cap: OK base=%d capped=%d\n", base, capped);
    return 0;
}
