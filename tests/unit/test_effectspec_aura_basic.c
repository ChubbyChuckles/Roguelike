/* EffectSpec Phase 6: Aura basic tests - entry/exit and pulse determinism.
   We spawn two enemies: one inside radius, one outside. Applying an AURA with
   duration=300 and period=100 should damage the inside enemy 3 times (t=0,100,200)
   and never touch the outside enemy. Then we move the second enemy inside radius
   between pulses and verify it starts taking damage only after entry.
*/
#include "../../src/core/app/app_state.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include "../../src/graphics/effect_spec_parser.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void init_minimal_world(void)
{
    memset(&g_app, 0, sizeof g_app);
    rogue_app_state_maybe_init();
    g_app.headless = 1;
    g_app.enemy_count = 2;
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 100;
    g_app.enemies[0].max_health = 100;
    g_app.enemies[0].base.pos.x = 0.5f; /* near player */
    g_app.enemies[0].base.pos.y = 0.0f;
    g_app.enemies[1].alive = 1;
    g_app.enemies[1].health = 100;
    g_app.enemies[1].max_health = 100;
    g_app.enemies[1].base.pos.x = 10.0f; /* far away */
    g_app.enemies[1].base.pos.y = 0.0f;
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;
    rogue_buffs_init();
}

int main(void)
{
    init_minimal_world();
    rogue_effect_reset();
    /* Define an aura: radius 2.0, magnitude 10, fire damage, pulses every 100ms for 300ms */
    RogueEffectSpec aura = {0};
    aura.kind = ROGUE_EFFECT_AURA;
    aura.magnitude = 10;
    aura.duration_ms = 300.0f;
    aura.pulse_period_ms = 100.0f;
    aura.aura_radius = 2.0f;
    aura.damage_type = ROGUE_DMG_FIRE;
    int id = rogue_effect_register(&aura);
    assert(id >= 0);

    double t = 0.0;
    /* Apply at t=0 => initial pulse */
    rogue_effect_apply(id, t);
    /* Enemy0 should be hit once */
    assert(g_app.enemies[0].health == 90);
    assert(g_app.enemies[1].health == 100);

    /* Advance to just before first pulse: no change */
    t = 99.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 90);
    assert(g_app.enemies[1].health == 100);

    /* At t=100 => second tick */
    t = 100.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 80);
    assert(g_app.enemies[1].health == 100);

    /* Move enemy1 inside radius before next tick and ensure it starts taking damage only then */
    g_app.enemies[1].base.pos.x = 0.0f; /* now at player */

    /* Next tick at t=200 */
    t = 200.0;
    rogue_effects_update(t);
    /* enemy0: -10 (70), enemy1: -10 (90) */
    assert(g_app.enemies[0].health == 70);
    assert(g_app.enemies[1].health == 90);

    /* Final tick at t=300 (inclusive) */
    t = 300.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 60);
    assert(g_app.enemies[1].health == 80);

    puts("EFFECTSPEC_AURA_ENTRY_EXIT_OK");
    return 0;
}
