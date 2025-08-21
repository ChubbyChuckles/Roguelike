#include "core/equipment/equipment_procs.h"
#include <assert.h>
#include <stdio.h>

static void register_sample_procs()
{
    rogue_procs_reset();
    RogueProcDef on_hit = {0};
    on_hit.trigger = ROGUE_PROC_ON_HIT;
    on_hit.icd_ms = 100;
    on_hit.duration_ms = 300;
    on_hit.stack_rule = ROGUE_PROC_STACK_REFRESH;
    on_hit.max_stacks = 0;
    rogue_proc_register(&on_hit);
    RogueProcDef on_crit = {0};
    on_crit.trigger = ROGUE_PROC_ON_CRIT;
    on_crit.icd_ms = 0;
    on_crit.duration_ms = 500;
    on_crit.stack_rule = ROGUE_PROC_STACK_STACK;
    on_crit.max_stacks = 5;
    rogue_proc_register(&on_crit);
}

int main(void)
{
    register_sample_procs();
    /* Simulate sequence: 10 hits with every 3rd crit over 2 seconds */
    int ms = 0;
    int crit_counter = 0;
    while (ms < 2000)
    {
        rogue_procs_event_hit((crit_counter % 3) == 0);
        crit_counter++;
        rogue_procs_update(100, 50, 100);
        ms += 100;
    }
    int hit_id = 0;
    int crit_id = 1;
    assert(rogue_proc_trigger_count(hit_id) > 0);
    assert(rogue_proc_trigger_count(crit_id) > 0);
    assert(rogue_proc_active_stacks(crit_id) >= 1);
    printf("equipment_phase6_proc_engine_basic_ok\n");
    return 0;
}
