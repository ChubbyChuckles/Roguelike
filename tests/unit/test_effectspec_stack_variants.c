/* Tests for EffectSpec stacking variants: multiplicative and replace_if_stronger. */
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <stdio.h>

int main(void)
{
    rogue_effect_reset();
    rogue_buffs_init();
    rogue_buffs_set_dampening(0.0); /* avoid dampening affecting rapid applies */

    /* Base additive buff to start from known magnitude */
    RogueEffectSpec base = {0};
    base.kind = ROGUE_EFFECT_STAT_BUFF;
    base.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    base.magnitude = 100;
    base.duration_ms = 1000.0f;
    base.stack_rule = ROGUE_BUFF_STACK_ADD; /* ensure a base active buff exists */
    int base_id = rogue_effect_register(&base);

    double now = 0.0;
    rogue_effect_apply(base_id, now);

    /* Multiplicative: apply 150% multiplier -> 100 -> 150 */
    RogueEffectSpec mult = {0};
    mult.kind = ROGUE_EFFECT_STAT_BUFF;
    mult.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    mult.magnitude = 150; /* percent */
    mult.duration_ms = 500.0f;
    mult.stack_rule = ROGUE_BUFF_STACK_MULTIPLY;
    int mult_id = rogue_effect_register(&mult);

    rogue_effect_apply(mult_id, now + 10.0);

    int total_after_mult = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    if (total_after_mult != 150)
    {
        printf("STACK_VARIANTS_FAIL mult expected=150 got=%d\n", total_after_mult);
        return 1;
    }

    /* Replace-if-stronger: incoming 120 should replace 150 -> 150 stays (no replace) */
    RogueEffectSpec repl = {0};
    repl.kind = ROGUE_EFFECT_STAT_BUFF;
    repl.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    repl.magnitude = 120;
    repl.duration_ms = 1200.0f; /* longer duration */
    repl.stack_rule = ROGUE_BUFF_STACK_REPLACE_IF_STRONGER;
    int repl_id = rogue_effect_register(&repl);

    rogue_effect_apply(repl_id, now + 20.0);

    int after_repl1 = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    if (after_repl1 != 150)
    {
        printf("STACK_VARIANTS_FAIL replace_weak expected=150 got=%d\n", after_repl1);
        return 1;
    }

    /* Stronger incoming 200 should replace 150 -> 200 */
    repl.magnitude = 200;
    int repl2_id = rogue_effect_register(&repl);
    rogue_effect_apply(repl2_id, now + 30.0);

    int after_repl2 = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    if (after_repl2 != 200)
    {
        printf("STACK_VARIANTS_FAIL replace_strong expected=200 got=%d\n", after_repl2);
        return 1;
    }

    puts("EFFECTSPEC_STACK_VARIANTS_OK");
    return 0;
}
