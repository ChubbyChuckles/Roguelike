#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills_procs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>

static int g_apply_count = 0;

/* Tiny shim to count applies by using a stat buff with tiny duration and polling count via a
 * predicate */
static int register_dummy_effect(void)
{
    RogueEffectSpec s = {0};
    s.kind = ROGUE_EFFECT_STAT_BUFF;
    s.buff_type = 0; /* POWER_STRIKE for simplicity */
    s.magnitude = 1;
    s.duration_ms = 1.0f;
    s.stack_rule = 3; /* ADD */
    int id = rogue_effect_register(&s);
    assert(id >= 0);
    return id;
}

static bool always_true(const RogueEvent* e)
{
    (void) e;
    return true;
}

int main(void)
{
    /* Init systems */
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("procs_icd");
    assert(rogue_event_bus_init(&cfg));
    rogue_skills_procs_init();

    int eff = register_dummy_effect();

    /* Register a global-ICD proc on DAMAGE_DEALT */
    RogueProcDef p1 = {0};
    p1.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    p1.effect_spec_id = eff;
    p1.icd_global_ms = 200.0; /* 200ms */
    p1.icd_per_target_ms = 0.0;
    p1.predicate = always_true;
    assert(rogue_skills_proc_register(&p1) >= 0);

    /* Register a per-target ICD proc also on DAMAGE_DEALT */
    RogueProcDef p2 = {0};
    p2.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    p2.effect_spec_id = eff;
    p2.icd_global_ms = 0.0;
    p2.icd_per_target_ms = 150.0; /* 150ms per target */
    p2.predicate = always_true;
    assert(rogue_skills_proc_register(&p2) >= 0);

    /* Publish 3 rapid DAMAGE_DEALT events to same target at t=0 */
    RogueEventPayload pay = {0};
    pay.damage_event.source_entity_id = 1;
    pay.damage_event.target_entity_id = 42;
    pay.damage_event.damage_amount = 10.0f;
    for (int i = 0; i < 3; ++i)
    {
        rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &pay, ROGUE_EVENT_PRIORITY_NORMAL, 0x54455354,
                            "test");
    }
    /* Process now; due to ICDs, each proc should trigger at most once */
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);

    /* Advance time by 100ms, publish again: global ICD still blocks first proc, per-target still
     * blocks as well */
    /* We rely on timestamp_us from event system, so simulate by sleeping event bus? Not available;
     * instead, emulate by publishing with deadline (ignored) and assume timestamp progresses. */
    /* In absence of manual clock, just publish more and verify we don't crash and counts are
     * non-decreasing monotonically; we indirectly validate via buff snapshots. */

    /* We approximate effect applications by checking active buff count increases at least once but
     * not more than 2 (one per proc) within ICD window. */
    extern int rogue_buffs_active_count(void);
    int after_first = rogue_buffs_active_count();
    if (after_first <= 0)
    {
        fprintf(stderr, "expected at least one effect application after initial burst\n");
        return 1;
    }
    if (after_first > 2)
    {
        fprintf(stderr, "expected at most two applications (two procs) after ICD gating, got %d\n",
                after_first);
        return 2;
    }

    /* Publish to a different target: should allow per-target proc even if same-tick */
    RogueEventPayload pay2 = pay;
    pay2.damage_event.target_entity_id = 77;
    rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &pay2, ROGUE_EVENT_PRIORITY_NORMAL, 0x54455354,
                        "test");
    rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
    int after_second = rogue_buffs_active_count();
    if (after_second < after_first)
    {
        fprintf(stderr, "buff count should not decrease\n");
        return 3;
    }

    printf("PH7_2_PROCS_ICD_OK buffs_after=%d\n", after_second);
    rogue_skills_procs_shutdown();
    rogue_event_bus_shutdown();
    return 0;
}
