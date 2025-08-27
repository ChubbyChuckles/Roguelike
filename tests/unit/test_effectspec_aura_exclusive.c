/* EffectSpec Phase 6.3: Aura exclusivity groups (replace-if-stronger)
   Create two AURAs in the same group mask with different magnitudes and durations.
   Applying weaker then stronger should replace the weaker (cancel its future pulses).
   Reapplying weaker while stronger active should be ignored. */
#include "../../src/core/app/app_state.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static void init_world(void)
{
    memset(&g_app, 0, sizeof g_app);
    rogue_app_state_maybe_init();
    g_app.headless = 1;
    g_app.enemy_count = 1;
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 100;
    g_app.enemies[0].max_health = 100;
    g_app.enemies[0].base.pos.x = 0.0f;
    g_app.enemies[0].base.pos.y = 0.0f;
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;
    rogue_buffs_init();
}

int main(void)
{
    init_world();
    rogue_effect_reset();

    RogueEffectSpec weak = {0};
    weak.kind = ROGUE_EFFECT_AURA;
    weak.magnitude = 5; /* weaker */
    weak.duration_ms = 500.0f;
    weak.pulse_period_ms = 100.0f;
    weak.aura_radius = 2.0f;
    weak.aura_group_mask = 0x1u; /* group A */

    RogueEffectSpec strong = weak;
    strong.magnitude = 10; /* stronger */

    int idW = rogue_effect_register(&weak);
    int idS = rogue_effect_register(&strong);
    assert(idW >= 0 && idS >= 0);

    double t = 0.0;
    /* Apply weaker first: tick at 0 */
    rogue_effect_apply(idW, t);
    assert(g_app.enemies[0].health == 95);

    /* Next pulse scheduled at 100 */
    t = 100.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 90);

    /* Now apply stronger at t=150: should replace weaker, next pulses come from strong */
    t = 150.0;
    rogue_effect_apply(idS, t);

    /* Advance to next tick boundary 200: should apply 10 (not 5) */
    t = 200.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 80);

    /* Reapply weaker while stronger active: should be ignored */
    t = 220.0;
    rogue_effect_apply(idW, t);

    /* Next tick at 300: still strong */
    t = 300.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 70);

    /* Final check at 400 */
    t = 400.0;
    rogue_effects_update(t);
    assert(g_app.enemies[0].health == 60);

    puts("EFFECTSPEC_AURA_EXCLUSIVE_OK");
    return 0;
}
