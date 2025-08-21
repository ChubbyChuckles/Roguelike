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

/* Attack with enough base to produce multi-component infusion */
static const RogueAttackDef g_attack = {0,
                                        "comp_event",
                                        ROGUE_WEAPON_LIGHT,
                                        0,
                                        0,
                                        60,
                                        0,
                                        8,
                                        0,
                                        30,
                                        ROGUE_DMG_PHYSICAL,
                                        0.4f,
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
    player.strength = 40;
    player.dexterity = 10;
    player.intelligence = 5;
    player.facing = 2;
    player.equipped_weapon_id = 0;
    player.weapon_infusion = 1; /* assume 1=fire infusion from table */
    g_exposed_player_for_stats = player;
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof enemy);
    enemy.alive = 1;
    enemy.team_id = 1;
    enemy.base.pos.x = 1.0f;
    enemy.base.pos.y = 0.0f;
    enemy.health = 500;
    enemy.max_health = 500;
    enemy.facing = 1;

    rogue_damage_events_clear();
    int dmg = strike_once(&pc, &player, &enemy);
    (void) dmg;
    RogueDamageEvent evs[16];
    int n = rogue_damage_events_snapshot(evs, 16);
    int physical_events = 0, fire_events = 0;
    for (int i = 0; i < n; i++)
    {
        if (evs[i].damage_type == ROGUE_DMG_PHYSICAL)
            physical_events++;
        else if (evs[i].damage_type == ROGUE_DMG_FIRE)
            fire_events++;
    }
    /* Expect at least one physical and one fire (components) plus one extra summary (which will
     * share the original attack damage_type = physical). So physical_events should be >=2 when
     * infusion active. */
    if (physical_events < 2 || fire_events < 1)
    {
        printf("fail_component_counts phys=%d fire=%d total=%d\n", physical_events, fire_events, n);
        return 1;
    }
    printf("phase7_damage_event_components: OK total=%d phys=%d fire=%d\n", n, physical_events,
           fire_events);
    return 0;
}
