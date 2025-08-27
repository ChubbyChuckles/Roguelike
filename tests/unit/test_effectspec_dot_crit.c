#include "../../src/core/app/app_state.h"
#include "../../src/game/combat.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    rogue_effect_reset();
    rogue_damage_events_clear();
    memset(&g_app, 0, sizeof g_app);
    g_app.enemy_count = 1;
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 100;
    g_app.enemies[0].max_health = 100;
    g_app.enemies[0].resist_physical = 0;

    RogueEffectSpec dot = {0};
    dot.kind = ROGUE_EFFECT_DOT;
    dot.magnitude = 20; /* base */
    dot.duration_ms = 1.0f;
    dot.pulse_period_ms = 1.0f; /* produce initial + one pulse within duration */
    dot.damage_type = ROGUE_DMG_PHYSICAL;
    int id = rogue_effect_register(&dot);

    extern int g_force_crit_mode; /* from combat */
    g_force_crit_mode = 1;        /* force crits */

    rogue_effect_apply(id, 0.0);
    /* crit -> 150% = 30 */
    assert(g_app.enemies[0].health == 70);
    rogue_effects_update(1.0);
    assert(g_app.enemies[0].health == 40);

    RogueDamageEvent evs[8];
    int n = rogue_damage_events_snapshot(evs, 8);
    assert(n == 2);
    assert(evs[0].crit == 1 && evs[1].crit == 1);

    puts("EFFECTSPEC_DOT_CRIT_OK");
    return 0;
}
