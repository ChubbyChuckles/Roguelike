#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void reset_all(void)
{
    rogue_effect_reset();
    rogue_buffs_init();
    /* Disable dampening so back-to-back applies at the same timestamp are not suppressed. */
    rogue_buffs_set_dampening(0.0);
}

int main(void)
{
    reset_all();
    /* Seed a baseline STRENGTH buff of 2 for 1000ms */
    int ok = rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH, 2, 1000.0, 0.0, ROGUE_BUFF_STACK_ADD, 0);
    assert(ok == 1);

    /* Spec A requires STRENGTH >= 3 (will be gated off) */
    RogueEffectSpec a;
    memset(&a, 0, sizeof a);
    a.kind = ROGUE_EFFECT_STAT_BUFF;
    a.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    a.magnitude = 5;
    a.duration_ms = 1000.0f;
    a.require_buff_type = ROGUE_BUFF_STAT_STRENGTH;
    a.require_buff_min = 3;
    int id_a = rogue_effect_register(&a);
    assert(id_a >= 0);

    /* Spec B requires STRENGTH >= 2 (will pass) */
    RogueEffectSpec b;
    memset(&b, 0, sizeof b);
    b.kind = ROGUE_EFFECT_STAT_BUFF;
    b.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    b.magnitude = 1;
    b.duration_ms = 1000.0f;
    b.require_buff_type = ROGUE_BUFF_STAT_STRENGTH;
    b.require_buff_min = 2;
    int id_b = rogue_effect_register(&b);
    assert(id_b >= 0);

    /* Apply both at t=0; only B should land (2+1=3) */
    rogue_effect_apply(id_a, 0.0);
    rogue_effect_apply(id_b, 0.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 3);

    /* Now schedule two children at the same timestamp and ensure deterministic order. */
    reset_all();
    RogueEffectSpec c1;
    memset(&c1, 0, sizeof c1);
    c1.kind = ROGUE_EFFECT_STAT_BUFF;
    c1.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    c1.magnitude = 1;
    c1.duration_ms = 1000.0f;
    int id_c1 = rogue_effect_register(&c1);
    RogueEffectSpec c2;
    memset(&c2, 0, sizeof c2);
    c2.kind = ROGUE_EFFECT_STAT_BUFF;
    c2.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    c2.magnitude = 2;
    c2.duration_ms = 1000.0f;
    int id_c2 = rogue_effect_register(&c2);

    RogueEffectSpec p;
    memset(&p, 0, sizeof p);
    p.kind = ROGUE_EFFECT_STAT_BUFF;
    p.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    p.magnitude = 0;
    p.duration_ms = 10.0f;
    p.child_count = 2;
    p.children[0].child_effect_id = id_c1;
    p.children[0].delay_ms = 50.0f;
    p.children[1].child_effect_id = id_c2;
    p.children[1].delay_ms = 50.0f;
    int id_p = rogue_effect_register(&p);

    rogue_effect_apply(id_p, 0.0);
    /* Before 50ms nothing has applied */
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 0);
    /* At exactly 50ms, both fire; deterministic ordering should still result in total 3. */
    rogue_effects_update(50.0);
    int total = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    assert(total == 3);

    printf("EFFECTSPEC_PRECOND_ORDER_OK\n");
    return 0;
}
