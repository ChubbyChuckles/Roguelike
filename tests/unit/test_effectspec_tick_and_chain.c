/* Tests for EffectSpec Phase 3: stacking rules and periodic pulses + child chaining */
#include "../../src/core/app/app_state.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_world(void)
{
    rogue_effect_reset();
    rogue_buffs_init();
    g_app.player.base.pos.x = 0;
    g_app.player.base.pos.y = 0;
}

int main(void)
{
    reset_world();
    /* Register a periodic strength buff that pulses every 100ms for 300ms total */
    RogueEffectSpec periodic;
    memset(&periodic, 0, sizeof periodic);
    periodic.kind = ROGUE_EFFECT_STAT_BUFF;
    periodic.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    periodic.magnitude = 1;
    periodic.duration_ms = 300.0f;
    periodic.pulse_period_ms = 100.0f;
    periodic.stack_rule = ROGUE_BUFF_STACK_ADD;
    int id_periodic = rogue_effect_register(&periodic);
    assert(id_periodic >= 0);

    /* Apply at t=0 and advance time to trigger pulses */
    rogue_effect_apply(id_periodic, 0.0);
    /* initial apply adds 1 */
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 1);
    /* tick at 100,200,300 should add up to 4 total */
    rogue_effects_update(100.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 2);
    rogue_effects_update(200.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 3);
    rogue_effects_update(300.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 4);

    /* Now test child chaining: parent schedules child after 50ms */
    reset_world();
    RogueEffectSpec child;
    memset(&child, 0, sizeof child);
    child.kind = ROGUE_EFFECT_STAT_BUFF;
    child.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    child.magnitude = 5;
    child.duration_ms = 1000.0f;
    int child_id = rogue_effect_register(&child);

    RogueEffectSpec parent;
    memset(&parent, 0, sizeof parent);
    parent.kind = ROGUE_EFFECT_STAT_BUFF;
    parent.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    parent.magnitude = 2;
    parent.duration_ms = 1000.0f;
    parent.child_count = 1;
    parent.children[0].child_effect_id = child_id;
    parent.children[0].delay_ms = 50.0f;
    int parent_id = rogue_effect_register(&parent);

    rogue_effect_apply(parent_id, 0.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 2);
    /* child not yet applied */
    rogue_effects_update(49.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 2);
    /* at 50ms child should apply */
    rogue_effects_update(50.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 7);

    printf("EFFECTSPEC_TICK_CHAIN_OK\n");
    return 0;
}
