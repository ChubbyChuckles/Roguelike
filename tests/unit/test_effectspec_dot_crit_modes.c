/* Validate crit modes: per-application snapshot vs per-tick RNG */
#include "../../src/core/app/app_state.h"
#include "../../src/game/combat.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_world(void)
{
    rogue_effect_reset();
    rogue_damage_events_clear();
    memset(&g_app, 0, sizeof g_app);
    g_app.enemy_count = 1;
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 1000;
    g_app.enemies[0].max_health = 1000;
    g_app.enemies[0].resist_physical = 0;
}

int main(void)
{
    /* Per-application snapshot: both ticks should have identical crit flag. */
    reset_world();
    RogueEffectSpec d1 = (RogueEffectSpec){0};
    d1.kind = ROGUE_EFFECT_DOT;
    d1.magnitude = 10;
    d1.duration_ms = 10.0f;
    d1.pulse_period_ms = 5.0f; /* 0 and 5 */
    d1.damage_type = ROGUE_DMG_PHYSICAL;
    d1.crit_mode = 1;         /* per-application */
    d1.crit_chance_pct = 100; /* always crit for deterministic check */
    int id1 = rogue_effect_register(&d1);
    rogue_effect_apply(id1, 0.0);
    RogueDamageEvent evs1[8];
    int n1 = rogue_damage_events_snapshot(evs1, 8);
    assert(n1 == 1);
    assert(evs1[0].crit == 1);
    rogue_effects_update(5.0);
    n1 = rogue_damage_events_snapshot(evs1, 8);
    assert(n1 == 2);
    assert(evs1[1].crit == 1); /* still crit */

    /* Per-tick: with 0% chance ensure both are non-crit and independent */
    reset_world();
    RogueEffectSpec d2 = (RogueEffectSpec){0};
    d2.kind = ROGUE_EFFECT_DOT;
    d2.magnitude = 10;
    d2.duration_ms = 10.0f;
    d2.pulse_period_ms = 5.0f;
    d2.damage_type = ROGUE_DMG_PHYSICAL;
    d2.crit_mode = 0;       /* per-tick */
    d2.crit_chance_pct = 0; /* always no-crit */
    int id2 = rogue_effect_register(&d2);
    rogue_effect_apply(id2, 0.0);
    RogueDamageEvent evs2[8];
    int n2 = rogue_damage_events_snapshot(evs2, 8);
    assert(n2 == 1);
    assert(evs2[0].crit == 0);
    rogue_effects_update(5.0);
    n2 = rogue_damage_events_snapshot(evs2, 8);
    assert(n2 == 2);
    assert(evs2[1].crit == 0);

    puts("EFFECTSPEC_DOT_CRIT_MODES_OK");
    return 0;
}
