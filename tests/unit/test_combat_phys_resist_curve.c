#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include "../../src/game/combat.h"
#include <assert.h>
#include <stdio.h>

/* Removed local rogue_app_add_hitstop / damage number stubs (link once in core). */
extern void rogue_app_add_hitstop(float ms);

int main(void)
{
    /* Basic smoke: ensure mitigation curve produces decreasing damage as armor increases */
    RoguePlayer p;
    rogue_player_init(&p);
    p.facing = 2;
    p.base.pos.x = 0;
    p.base.pos.y = 0;
    RoguePlayerCombat c;
    rogue_combat_init(&c);
    rogue_combat_set_archetype(&c, ROGUE_WEAPON_LIGHT);
    RogueEnemy e = {0};
    e.alive = 1;
    e.base.pos.x = 0.8f;
    e.health = e.max_health = 10000;
    int dmg_low = 0, dmg_high = 0;
    p.strength = 50;
    p.dexterity = 50;
    e.armor = 0;
    int before = e.health;
    rogue_combat_player_strike(&c, &p, &e, 1);
    dmg_low = before - e.health;
    e.health = e.max_health;
    e.armor = 200;
    before = e.health;
    rogue_combat_player_strike(&c, &p, &e, 1);
    dmg_high = before - e.health;
    assert(dmg_high < dmg_low);
    printf("combat_phys_resist_curve: OK (%d -> %d)\n", dmg_low, dmg_high);
    return 0;
}
