#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills_procs.h"
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>

static void bus_init(void)
{
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("proc_phase7_6");
    assert(rogue_event_bus_init(&cfg));
}

static void bus_shutdown(void) { rogue_event_bus_shutdown(); }

static void publish_damage(uint32_t src, uint32_t target)
{
    RogueEventPayload pay = {0};
    pay.damage_event.source_entity_id = src;
    pay.damage_event.target_entity_id = target;
    pay.damage_event.damage_amount = 1.0f;
    (void) rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &pay, ROGUE_EVENT_PRIORITY_NORMAL,
                               0x50373652 /* 'P76R' */, "test_skills_phase7_6");
    (void) rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
}

static int register_statbuff_effect(int magnitude)
{
    RogueEffectSpec spec = {0};
    spec.kind = ROGUE_EFFECT_STAT_BUFF;
    spec.buff_type = ROGUE_BUFF_STAT_STRENGTH;
    spec.magnitude = magnitude;
    spec.duration_ms = 1000.0f;
    spec.stack_rule = 3; /* ROGUE_BUFF_STACK_ADD */
    return rogue_effect_register(&spec);
}

static void setup_common(void)
{
    rogue_skills_procs_reset();
    rogue_buffs_init();
    rogue_buffs_set_dampening(0.0); /* allow rapid re-apply for deterministic counts */
    rogue_effect_reset();
}

static void test_icd_global_blocks_immediate_second_trigger(void)
{
    setup_common();
    int eff = register_statbuff_effect(1);
    assert(eff >= 0);

    RogueProcDef def = {0};
    def.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    def.effect_spec_id = eff;
    def.icd_global_ms = 100000.0; /* huge: only first trigger allowed immediately */
    def.icd_per_target_ms = 0.0;
    def.chance_pct =
        100; /* first trigger guaranteed; second may roll after scaling but ICD should block */
    def.use_smoothing = 0;
    int pid = rogue_skills_proc_register(&def);
    assert(pid >= 0);

    int before = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    publish_damage(1, 7);
    publish_damage(1, 7);
    int after = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);

    assert(after == before + 1);
}

static void test_icd_per_target_allows_different_targets(void)
{
    setup_common();
    int eff = register_statbuff_effect(1);
    assert(eff >= 0);

    RogueProcDef def = {0};
    def.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    def.effect_spec_id = eff;
    def.icd_global_ms = 0.0;
    def.icd_per_target_ms = 100000.0; /* per-target gating only */
    def.chance_pct = 100;             /* ensure no roll for both immediate triggers */
    def.use_smoothing = 0;
    int pid = rogue_skills_proc_register(&def);
    assert(pid >= 0);

    int before = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    publish_damage(1, 101); /* target A */
    publish_damage(1, 202); /* target B */
    int after = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);

    /* Different targets -> both should apply */
    assert(after == before + 2);
}

static void test_smoothing_guarantees_eventual_trigger(void)
{
    setup_common();
    int eff = register_statbuff_effect(1);
    assert(eff >= 0);

    RogueProcDef def = {0};
    def.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    def.effect_spec_id = eff;
    def.icd_global_ms = 0.0;
    def.icd_per_target_ms = 0.0;
    def.chance_pct = 10;   /* low base chance */
    def.use_smoothing = 1; /* enable smoothing accumulator */
    int pid = rogue_skills_proc_register(&def);
    assert(pid >= 0);

    int before = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    int applied = 0;
    for (int i = 0; i < 10; ++i)
    {
        publish_damage(1, 404);
        int cur = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
        if (cur > before)
        {
            applied = 1;
            break;
        }
    }
    assert(applied == 1); /* should trigger within a small number of attempts */
}

int main(void)
{
    bus_init();

    test_icd_global_blocks_immediate_second_trigger();
    test_icd_per_target_allows_different_targets();
    test_smoothing_guarantees_eventual_trigger();

    bus_shutdown();
    return 0;
}
