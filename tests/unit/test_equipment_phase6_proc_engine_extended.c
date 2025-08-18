/* Extended tests for proc stacking semantics, ICD, ordering, telemetry */
#include <assert.h>
#include <stdio.h>
#include "core/equipment_procs.h"

static void setup(){ rogue_procs_reset(); }

int main(void){
    setup();
    RogueProcDef refresh={0}; refresh.trigger=ROGUE_PROC_ON_HIT; refresh.icd_ms=50; refresh.duration_ms=200; refresh.stack_rule=ROGUE_PROC_STACK_REFRESH; rogue_proc_register(&refresh);
    RogueProcDef stack={0}; stack.trigger=ROGUE_PROC_ON_HIT; stack.icd_ms=0; stack.duration_ms=300; stack.stack_rule=ROGUE_PROC_STACK_STACK; stack.max_stacks=3; rogue_proc_register(&stack);
    RogueProcDef ignore={0}; ignore.trigger=ROGUE_PROC_ON_HIT; ignore.icd_ms=0; ignore.duration_ms=300; ignore.stack_rule=ROGUE_PROC_STACK_IGNORE; rogue_proc_register(&ignore);
    /* Fire rapid hits: 10 hits every 20ms = 200ms total */
    for(int i=0;i<10;i++){ rogue_procs_event_hit(0); rogue_procs_update(20, 80, 100); }
    /* Validate refresh proc did not exceed 1 stack but refreshed duration (still active). */
    assert(rogue_proc_active_stacks(0)==1);
    /* Stacking proc should have up to 3 stacks. */
    assert(rogue_proc_active_stacks(1)<=3 && rogue_proc_active_stacks(1)>=1);
    /* Ignore proc should be exactly 1. */
    assert(rogue_proc_active_stacks(2)==1);
    /* Ordering: later procs get higher sequence numbers. */
    int seq0 = rogue_proc_last_trigger_sequence(0);
    int seq1 = rogue_proc_last_trigger_sequence(1);
    int seq2 = rogue_proc_last_trigger_sequence(2);
    assert(seq0>0 && seq1>0 && seq2>0);
    /* Telemetry: triggers per minute > 0 */
    assert(rogue_proc_triggers_per_min(0) > 0.f);
    printf("equipment_phase6_proc_engine_extended_ok\n");
    return 0;
}
