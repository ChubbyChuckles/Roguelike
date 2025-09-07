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
    /* Deterministic test: verify that increasing physical resistance reduces applied damage.
        We bypass the strike system and call mitigation directly to isolate the curve logic. */
    RogueEnemy e = {0};
    e.alive = 1;
    e.max_health = 100000;
    e.health = e.max_health;
    e.armor = 0; /* isolate percent resist only */
    int overkill = 0;
    int raw = 1200; /* large enough to exercise diminishing returns region */
    e.resist_physical = 0;
    int dmg_r0 = rogue_apply_mitigation_enemy(&e, raw, ROGUE_DMG_PHYSICAL, &overkill);
    /* reset health for second application */
    e.health = e.max_health;
    e.resist_physical = 90; /* near cap (maps to ~70 effective, capped 75) */
    int dmg_r90 = rogue_apply_mitigation_enemy(&e, raw, ROGUE_DMG_PHYSICAL, &overkill);
    assert(dmg_r90 < dmg_r0);
    printf("combat_phys_resist_curve: OK (r0=%d r90=%d)\n", dmg_r0, dmg_r90);
    return 0;
}
