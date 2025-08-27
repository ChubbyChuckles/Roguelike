/* Validate deterministic ordering when a pulse and a child fire at the same timestamp.
 * Expectation: pulse events are processed before child events at identical when_ms.
 * Scenario: baseline STR=10; at t=0 apply a parent MULTIPLY(200%) with pulse at 100ms and a
 * child ADD(+5) scheduled for 100ms. Immediate apply at t=0 multiplies 10->20.
 * At t=100ms, if pulse runs before child: (20 * 2) + 5 = 45. If reversed: (20 + 5) * 2 = 50.
 * We assert 45 to lock ordering.
 */
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_all(void)
{
    rogue_effect_reset();
    rogue_buffs_init();
    /* Disable dampening to avoid suppressing rapid back-to-back applies in tests. */
    rogue_buffs_set_dampening(0.0);
}

int main(void)
{
    reset_all();

    /* Baseline STR +10 for long enough duration */
    RogueEffectSpec base = {0};
    base.kind = ROGUE_EFFECT_STAT_BUFF;
    base.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    base.magnitude = 10;
    base.duration_ms = 1000.0f;
    base.stack_rule = ROGUE_BUFF_STACK_ADD;
    int base_id = rogue_effect_register(&base);
    assert(base_id >= 0);

    /* Child: +5 ADD at 100ms */
    RogueEffectSpec child = {0};
    child.kind = ROGUE_EFFECT_STAT_BUFF;
    child.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    child.magnitude = 5;
    child.duration_ms = 1000.0f;
    child.stack_rule = ROGUE_BUFF_STACK_ADD;
    int child_id = rogue_effect_register(&child);
    assert(child_id >= 0);

    /* Parent: MULTIPLY 200%, pulses every 100ms; also schedules child at 100ms */
    RogueEffectSpec parent = {0};
    parent.kind = ROGUE_EFFECT_STAT_BUFF;
    parent.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    parent.magnitude = 200; /* percent multiplier */
    parent.duration_ms = 1000.0f;
    parent.stack_rule = ROGUE_BUFF_STACK_MULTIPLY;
    parent.pulse_period_ms = 100.0f;
    parent.child_count = 1;
    parent.children[0].child_effect_id = child_id;
    parent.children[0].delay_ms = 100.0f;
    int parent_id = rogue_effect_register(&parent);
    assert(parent_id >= 0);

    /* Apply baseline then parent at t=0 */
    rogue_effect_apply(base_id, 0.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 10);
    rogue_effect_apply(parent_id, 0.0);
    /* Immediate multiply 200%: 10 -> 20 */
    int after_apply = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    if (after_apply != 20)
    {
        printf("PULSE_CHILD_ORDER_FAIL immediate mult expected=20 got=%d\n", after_apply);
        return 1;
    }

    /* Advance to 100ms when both pulse and child land at the same timestamp. */
    rogue_effects_update(100.0);
    int total = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    if (total != 45)
    {
        printf("PULSE_CHILD_ORDER_FAIL expected=45 got=%d\n", total);
        return 2;
    }

    puts("EFFECTSPEC_PULSE_CHILD_ORDER_OK");
    return 0;
}
