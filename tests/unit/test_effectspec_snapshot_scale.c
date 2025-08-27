/* Validate EffectSpec Phase 3.4: per-attribute snapshot vs dynamic scaling */
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <string.h>

static void reset_all(void)
{
    rogue_effect_reset();
    rogue_buffs_init();
    rogue_buffs_set_dampening(0.0);
}

int main(void)
{
    reset_all();
    /* Baseline STR=1 */
    rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH, 1, 1000.0, 0.0, ROGUE_BUFF_STACK_ADD, 0);

    /* Define two effects that scale by STR at +100% per point from base magnitude 5. */
    RogueEffectSpec snap = {0};
    snap.kind = ROGUE_EFFECT_STAT_BUFF;
    snap.buff_type = ROGUE_BUFF_POWER_STRIKE; /* write into POWER_STRIKE to observe magnitude */
    snap.magnitude = 5;
    snap.duration_ms = 500.0f;
    snap.pulse_period_ms = 250.0f; /* one pulse at 250ms and another at 500ms inclusive */
    snap.scale_by_buff_type = ROGUE_BUFF_STAT_STRENGTH;
    snap.scale_pct_per_point = 100; /* +100% per STR */
    snap.snapshot_scale = 1;        /* snapshot at apply */
    int id_snap = rogue_effect_register(&snap);

    RogueEffectSpec dyn = snap;
    dyn.snapshot_scale = 0; /* dynamic */
    int id_dyn = rogue_effect_register(&dyn);

    /* Apply both at t=0. With STR=1, eff mag should be 10 on initial apply. */
    rogue_effect_apply(id_snap, 0.0);
    rogue_effect_apply(id_dyn, 0.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE) == 20); /* 10 + 10 */

    /* Increase STR to 2 at t=100ms. */
    rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH, 1, 1000.0, 100.0, ROGUE_BUFF_STACK_ADD, 0);

    /* At 250ms: snap pulse remains 10; dyn pulse becomes 15 (base 5 * (1 + 2*100%) = 15). */
    rogue_effects_update(250.0);
    int after_250 = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
    assert(after_250 == 45); /* 20 + 10 + 15 */

    /* At 500ms: again add 10 (snap) + 15 (dyn) */
    rogue_effects_update(500.0);
    int after_500 = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
    assert(after_500 == 70);

    return 0;
}
