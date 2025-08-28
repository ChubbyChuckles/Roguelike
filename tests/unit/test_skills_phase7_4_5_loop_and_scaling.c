#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills_procs.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>

/* Minimal harness: register a proc that triggers on DAMAGE_DEALT and increments a buff.
   We simulate a burst of rapid DAMAGE_DEALT events to validate:
   - loop guard does not deadlock (depth or cycle), callback returns
   - dynamic scaling can lower trigger chance after multiple hits within 1s (non-strict) */

static void reset_effects(void) { rogue_effect_reset(); }

static int register_statbuff_effect(int magnitude)
{
    RogueEffectSpec spec = {0};
    spec.kind = ROGUE_EFFECT_STAT_BUFF;
    spec.buff_type = ROGUE_BUFF_STAT_STRENGTH; /* use a defined buff type */
    spec.magnitude = magnitude;
    spec.duration_ms = 1000.0f;
    spec.stack_rule = 3; /* ROGUE_BUFF_STACK_ADD */
    return rogue_effect_register(&spec);
}

static void publish_damage(uint32_t src, uint32_t target)
{
    RogueEventPayload pay = {0};
    pay.damage_event.source_entity_id = src;
    pay.damage_event.target_entity_id = target;
    pay.damage_event.damage_amount = 1.0f;
    (void) rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &pay, ROGUE_EVENT_PRIORITY_NORMAL,
                               0x4C4F4F50 /* 'LOOP' */, "test_loop_scale");
    (void) rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
}

int main(void)
{
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("proc_loop_scale");
    assert(rogue_event_bus_init(&cfg));
    rogue_skills_procs_init();
    /* Ensure buff system is clean and allow rapid re-applications for this test */
    rogue_buffs_init();
    rogue_buffs_set_dampening(0.0);
    reset_effects();

    int eff = register_statbuff_effect(1);
    assert(eff >= 0);

    RogueProcDef def = {0};
    def.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    def.effect_spec_id = eff;
    def.icd_global_ms = 0.0;
    def.icd_per_target_ms = 0.0;
    def.chance_pct = 100; /* always trigger initially; scaling will reduce later */
    def.use_smoothing = 0;
    int pid = rogue_skills_proc_register(&def);
    assert(pid >= 0);

    /* Simulate a rapid burst: 6 damage events back-to-back */
    int before = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    for (int i = 0; i < 6; ++i)
    {
        publish_damage(1, 42);
    }
    int after = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);

    /* Guarantees: at least the first two will apply (100% before scaling), and never exceed 6. */
    assert(after >= before + 2);
    assert(after <= before + 6);

    rogue_skills_procs_shutdown();
    rogue_event_bus_shutdown();
    /* Ensure we didn't deadlock or crash due to loop guard. */
    printf("applies=%d\n", after - before);
    return 0;
}
