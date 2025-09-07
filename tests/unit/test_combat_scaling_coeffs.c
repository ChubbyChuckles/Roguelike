#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>

extern void rogue_app_add_hitstop(float ms);

/* Stubs */
RoguePlayer g_exposed_player_for_stats = {0};
static int rogue_get_current_attack_frame(void) { return 3; }
/* NOTE: Removed local hitstop/damage number stubs; rely on core implementations to
 * avoid duplicate symbol linkage on MSVC. */

static int strike_damage_once(RogueWeaponArchetype arch, int str, int dex, int intel)
{
    RoguePlayer p;
    rogue_player_init(&p);
    p.strength = str;
    p.dexterity = dex;
    p.intelligence = intel;
    p.base.pos.x = 0;
    p.base.pos.y = 0;
    p.facing = 2;
    p.crit_chance = 0;
    p.crit_damage = 0;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, arch);
    c.phase = ROGUE_ATTACK_STRIKE;
    c.combo = 0; /* direct strike */
    RogueEnemy e = (RogueEnemy){0};
    e.alive = 1;
    e.base.pos.x = 0.8f;
    e.base.pos.y = 0;
    e.health = 10000;
    e.max_health = 10000;
    int before = e.health;
    rogue_combat_player_strike(&c, &p, &e, 1);
    return before - e.health;
}

int main()
{
    int dmg_light_str = strike_damage_once(ROGUE_WEAPON_LIGHT, 80, 10, 10);
    int dmg_light_dex = strike_damage_once(ROGUE_WEAPON_LIGHT, 10, 80, 10);
    printf("light str80=%d dex80=%d\n", dmg_light_str, dmg_light_dex);
    /* Current authored coeffs produce equal light archetype damage for STR vs DEX (baseline test
        asserts non-regression: STR should not be lower than DEX). */
    assert(dmg_light_str >= dmg_light_dex);

    int dmg_thrust_str = strike_damage_once(ROGUE_WEAPON_THRUST, 80, 10, 10);
    int dmg_thrust_dex = strike_damage_once(ROGUE_WEAPON_THRUST, 10, 80, 10);
    printf("thrust str80=%d dex80=%d\n", dmg_thrust_str, dmg_thrust_dex);
    assert(dmg_thrust_dex > dmg_thrust_str); /* thrust favors DEX */

    int dmg_spell_int = strike_damage_once(ROGUE_WEAPON_SPELL_FOCUS, 10, 10, 80);
    int dmg_spell_str = strike_damage_once(ROGUE_WEAPON_SPELL_FOCUS, 80, 10, 10);
    printf("spell int80=%d str80=%d\n", dmg_spell_int, dmg_spell_str);
    assert(dmg_spell_int > dmg_spell_str); /* spell focus favors INT */
    printf("combat_scaling_coeffs: OK\n");
    return 0;
}
