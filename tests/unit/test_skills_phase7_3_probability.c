#include "../../src/core/integration/event_bus.h"
#include "../../src/core/skills/skills_procs.h"
#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>

/* Register a trivial effect that increments active buff count briefly */
static int register_dummy_effect(void)
{
    RogueEffectSpec s = {0};
    s.kind = ROGUE_EFFECT_STAT_BUFF;
    s.buff_type = 0;
    s.magnitude = 1;
    s.duration_ms = 1.0f;
    s.stack_rule = 3; /* ADD */
    int id = rogue_effect_register(&s);
    assert(id >= 0);
    return id;
}

int main(void)
{
    RogueEventBusConfig cfg = rogue_event_bus_create_default_config("procs_p7_3");
    assert(rogue_event_bus_init(&cfg));
    rogue_skills_procs_init();

    int eff = register_dummy_effect();

    /* 25% chance with smoothing should eventually trigger within few events */
    RogueProcDef p = {0};
    p.event_type = ROGUE_EVENT_DAMAGE_DEALT;
    p.effect_spec_id = eff;
    p.icd_global_ms = 0.0;
    p.icd_per_target_ms = 0.0;
    p.chance_pct = 25;
    p.use_smoothing = 1;
    assert(rogue_skills_proc_register(&p) >= 0);

    RogueEventPayload pay = {0};
    pay.damage_event.source_entity_id = 1;
    pay.damage_event.target_entity_id = 99;
    pay.damage_event.damage_amount = 1.0f;

    extern int rogue_buffs_active_count(void);
    int before = rogue_buffs_active_count();

    int triggered = 0;
    for (int i = 0; i < 20 && !triggered; ++i)
    {
        rogue_event_publish(ROGUE_EVENT_DAMAGE_DEALT, &pay, ROGUE_EVENT_PRIORITY_NORMAL, 0x50373350,
                            "p7_3");
        rogue_event_process_priority(ROGUE_EVENT_PRIORITY_NORMAL, 100000);
        int now = rogue_buffs_active_count();
        if (now > before)
        {
            triggered = 1;
            break;
        }
    }
    if (!triggered)
    {
        fprintf(stderr, "expected at least one trigger with smoothing within 20 events\n");
        return 1;
    }

    rogue_skills_procs_shutdown();
    rogue_event_bus_shutdown();
    return 0;
}
