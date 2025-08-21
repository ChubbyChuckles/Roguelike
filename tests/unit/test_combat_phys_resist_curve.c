/* Tests diminishing returns curve for physical percent resist (Phase 2.2 finalization). */
#define SDL_MAIN_HANDLED
#include "game/combat.h"
#include <assert.h>
#include <stdio.h>

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void) { return 3; }
void rogue_app_add_hitstop(float ms) { (void) ms; }
void rogue_add_damage_number(float x, float y, int amount, int from_player)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
}
void rogue_add_damage_number_ex(float x, float y, int amount, int from_player, int crit)
{
    (void) x;
    (void) y;
    (void) amount;
    (void) from_player;
    (void) crit;
}

static int apply_once(int phys_resist_raw)
{
    RoguePlayer p;
    rogue_player_init(&p);
    p.base.pos.x = 0;
    p.base.pos.y = 0;
    p.facing = 2;
    p.strength = 80; /* stronger for higher base damage */
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    c.phase = ROGUE_ATTACK_STRIKE;
    c.archetype = ROGUE_WEAPON_LIGHT;
    c.chain_index = 0;
    c.strike_time_ms = 20.0f;
    RogueEnemy e;
    e.alive = 1;
    e.base.pos.x = 0.9f;
    e.base.pos.y = 0;
    e.max_health = 100000;
    e.health = 100000;
    e.armor = 0;
    e.resist_physical = phys_resist_raw;
    rogue_combat_player_strike(&c, &p, &e, 1);
    return 100000 - e.health; /* damage dealt */
}

int main()
{
    int d0 = apply_once(0);
    int d30 = apply_once(30);
    int d60 = apply_once(60);
    int d90 = apply_once(90);
    printf("dmg: r0=%d r30=%d r60=%d r90=%d\n", d0, d30, d60, d90);
    /* Ensure monotonic decrease */
    assert(d30 < d0);
    assert(d60 < d30);
    assert(d90 < d60);
    /* Diminishing returns: (d0 - d30) > (d30 - d60) and (d30 - d60) >= (d60 - d90) */
    int diff1 = d0 - d30;
    int diff2 = d30 - d60;
    int diff3 = d60 - d90;
    assert(diff1 > diff2);
    assert(diff2 >= diff3);
    printf("phys_resist_curve: OK\n");
    return 0;
}
