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
    g_app.enemies[0].health = 100;
    g_app.enemies[0].max_health = 100;
    g_app.enemies[0].resist_fire = 50; /* 50% resist to validate mitigation */
}

int main(void)
{
    reset_world();
    RogueEffectSpec dot = {0};
    dot.kind = ROGUE_EFFECT_DOT;
    dot.debuff = 1;
    dot.magnitude = 20; /* base per tick raw */
    dot.duration_ms = 500.0f;
    dot.pulse_period_ms = 250.0f; /* initial + 250 + 500 => 3 ticks total */
    dot.damage_type = ROGUE_DMG_FIRE;
    int id = rogue_effect_register(&dot);
    assert(id >= 0);

    /* Apply at t=0: tick 1 applies immediately */
    rogue_effect_apply(id, 0.0);
    /* 50% resist -> mitigated = 10 */
    assert(g_app.enemies[0].health == 90);
    /* Advance to 250 and 500 for two more ticks */
    rogue_effects_update(250.0);
    assert(g_app.enemies[0].health == 80);
    rogue_effects_update(500.0);
    assert(g_app.enemies[0].health == 70);

    /* Damage events should have 3 entries */
    RogueDamageEvent evs[8];
    int n = rogue_damage_events_snapshot(evs, 8);
    assert(n == 3);
    for (int i = 0; i < n; ++i)
        assert(evs[i].damage_type == ROGUE_DMG_FIRE);

    puts("EFFECTSPEC_DOT_BASIC_OK");
    return 0;
}
