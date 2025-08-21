#include "deadlock_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int g_aborted[16];
static int g_abort_count = 0;
static int abort_cb(int tx_id, const char* reason)
{
    (void) reason;
    if (g_abort_count < 16)
        g_aborted[g_abort_count++] = tx_id;
    return 0;
}

int main(void)
{
    rogue_deadlock_reset_all();
    rogue_deadlock_set_abort_callback(abort_cb);
    assert(rogue_deadlock_register_resource(1) == 0);
    assert(rogue_deadlock_register_resource(2) == 0);
    assert(rogue_deadlock_register_resource(3) == 0);
    assert(rogue_deadlock_acquire(101, 1) == 0);
    assert(rogue_deadlock_acquire(202, 2) == 0);
    assert(rogue_deadlock_acquire(303, 3) == 0);
    assert(rogue_deadlock_acquire(101, 2) == 1);
    assert(rogue_deadlock_acquire(202, 3) == 1);
    assert(rogue_deadlock_acquire(303, 1) == 1);
    int resolved = rogue_deadlock_tick(0);
    assert(resolved >= 1);
    RogueDeadlockStats st;
    rogue_deadlock_get_stats(&st);
    assert(st.deadlocks_detected >= 1);
    assert(st.victims_aborted >= 1);
    const RogueDeadlockCycle* cycles = NULL;
    size_t cc = 0;
    assert(rogue_deadlock_cycles_get(&cycles, &cc) == 0);
    assert(cc >= 1);
    int victim = cycles[cc - 1].victim_tx_id;
    int found = 0;
    for (int i = 0; i < g_abort_count; i++)
        if (g_aborted[i] == victim)
            found = 1;
    assert(found);
    rogue_deadlock_tick(1);
    size_t prev_detected = st.deadlocks_detected;
    rogue_deadlock_tick(2);
    RogueDeadlockStats st2;
    rogue_deadlock_get_stats(&st2);
    assert(st2.deadlocks_detected >= prev_detected);
    printf("[test_deadlock_manager] PASS cycles=%zu victim=%d aborts=%d\n", cc, victim,
           g_abort_count);
    return 0;
}
