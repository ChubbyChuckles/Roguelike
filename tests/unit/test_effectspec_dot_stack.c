/* DoT stacking tests: REFRESH vs EXTEND and UNIQUE rejection */
#include "../../src/core/app/app_state.h"
#include "../../src/game/buffs.h"
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
    g_app.enemies[0].resist_physical = 0;
}

int main(void)
{
    reset_world();
    /* Base dot does 10 dmg per tick, period=100, duration=200 => ticks at 0,100,200 */
    RogueEffectSpec dot = (RogueEffectSpec){0};
    dot.kind = ROGUE_EFFECT_DOT;
    dot.magnitude = 10;
    dot.duration_ms = 200.0f;
    dot.pulse_period_ms = 100.0f;
    dot.damage_type = ROGUE_DMG_PHYSICAL;
    dot.stack_rule = ROGUE_BUFF_STACK_REFRESH;
    int id_refresh = rogue_effect_register(&dot);

    /* Apply at t=0 => health 90 */
    rogue_effect_apply(id_refresh, 0.0);
    assert(g_app.enemies[0].health == 90);
    /* Reapply at t=150: refresh -> next ticks at 150,250,350 */
    rogue_effects_update(100.0);
    assert(g_app.enemies[0].health == 80);
    rogue_effect_apply(id_refresh, 150.0);
    assert(g_app.enemies[0].health == 70);
    rogue_effects_update(250.0); /* one tick at 250 */
    assert(g_app.enemies[0].health == 60);
    rogue_effects_update(350.0); /* final tick at 350 */
    assert(g_app.enemies[0].health == 50);

    /* Now EXTEND behavior */
    reset_world();
    dot.stack_rule = ROGUE_BUFF_STACK_EXTEND;
    int id_extend = rogue_effect_register(&dot);
    rogue_effect_apply(id_extend, 0.0); /* 90 */
    assert(g_app.enemies[0].health == 90);
    rogue_effects_update(100.0); /* 80 */
    assert(g_app.enemies[0].health == 80);
    rogue_effect_apply(id_extend, 150.0); /* apply again extends => immediate tick => 70 */
    assert(g_app.enemies[0].health == 70);
    /* Next pulses should occur at 200,300,400 (extended window) */
    rogue_effects_update(200.0);
    assert(g_app.enemies[0].health == 60);
    rogue_effects_update(300.0);
    assert(g_app.enemies[0].health == 50);
    rogue_effects_update(400.0);
    assert(g_app.enemies[0].health == 40);

    /* UNIQUE: second apply should be ignored */
    reset_world();
    dot.stack_rule = ROGUE_BUFF_STACK_UNIQUE;
    int id_unique = rogue_effect_register(&dot);
    rogue_effect_apply(id_unique, 0.0); /* 90 */
    assert(g_app.enemies[0].health == 90);
    rogue_effect_apply(id_unique, 50.0); /* ignored */
    assert(g_app.enemies[0].health == 90);
    rogue_effects_update(100.0); /* 80 */
    assert(g_app.enemies[0].health == 80);

    puts("EFFECTSPEC_DOT_STACK_OK");
    return 0;
}
