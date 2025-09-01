#include "../../src/core/app/app_state.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static int register_damage_spec(int magnitude, float delay_ms, int repeat_count,
                                float repeat_interval_ms, unsigned char caster_hp_le,
                                float max_distance)
{
    RogueEffectSpec es;
    memset(&es, 0, sizeof es);
    es.kind = ROGUE_EFFECT_DAMAGE;
    es.magnitude = magnitude;
    es.delay_ms = delay_ms;
    es.repeat_count = (unsigned char) repeat_count;
    es.repeat_interval_ms = repeat_interval_ms;
    es.caster_health_le_pct = caster_hp_le;
    es.max_distance = max_distance;
    return rogue_effect_register(&es);
}

int main(void)
{
    rogue_buffs_init();
    rogue_effect_reset();
    memset(&g_app, 0, sizeof g_app);
    g_app.enemy_count = 1;
    g_app.enemies[0].alive = 1;
    g_app.enemies[0].health = 100;
    g_app.enemies[0].max_health = 100;
    g_app.enemies[0].base.pos.x = 0.0f;
    g_app.enemies[0].base.pos.y = 0.0f;
    g_app.player.health = 50;
    g_app.player.max_health = 100;
    g_app.player.base.pos.x = 0.0f;
    g_app.player.base.pos.y = 0.0f;

    int id = register_damage_spec(10, 50.0f, 2, 25.0f, 60, 2.0f);
    /* The player HP is 50% <= 60 gate, distance 0 <= 2, so it should go through. */

    /* Apply at t=0 schedules at 50, 75, 100 */
    rogue_effect_apply(id, 0.0);
    assert(g_app.enemies[0].health == 100);
    rogue_effects_update(49.0);
    assert(g_app.enemies[0].health == 100);
    rogue_effects_update(50.0);
    assert(g_app.enemies[0].health == 90);
    rogue_effects_update(75.0);
    assert(g_app.enemies[0].health == 80);
    rogue_effects_update(100.0);
    assert(g_app.enemies[0].health == 70);

    /* Now test distance gating: move enemy far away, repeat should be blocked */
    g_app.enemies[0].health = 100;
    g_app.enemies[0].base.pos.x = 100.0f; /* far */
    g_app.enemies[0].base.pos.y = 0.0f;
    rogue_effect_apply(id, 200.0);
    rogue_effects_update(250.0);
    assert(g_app.enemies[0].health == 100);

    /* Test HP gate: raise player health above threshold, nothing should apply */
    g_app.enemies[0].base.pos.x = 0.0f;
    g_app.player.health = 100; /* 100% > 60 gate */
    rogue_effect_apply(id, 300.0);
    rogue_effects_update(400.0);
    assert(g_app.enemies[0].health == 100);

    printf("EFFECTSPEC_TIMING_AND_CONDITIONALS_OK\n");
    rogue_effect_reset();
    return 0;
}
