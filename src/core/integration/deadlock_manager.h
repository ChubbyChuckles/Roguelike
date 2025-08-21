// Deadlock Detection & Prevention (Phase 5.6)
// Provides resource dependency graph tracking, cycle detection, victim selection,
// resolution (abort + resource preemption), monitoring, debugging & stats.
#ifndef ROGUE_DEADLOCK_MANAGER_H
#define ROGUE_DEADLOCK_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ROGUE_DEADLOCK_MAX_RESOURCES
#define ROGUE_DEADLOCK_MAX_RESOURCES 128
#endif
#ifndef ROGUE_DEADLOCK_MAX_TX
#define ROGUE_DEADLOCK_MAX_TX 256
#endif
#ifndef ROGUE_DEADLOCK_MAX_WAITERS
#define ROGUE_DEADLOCK_MAX_WAITERS 16
#endif
#ifndef ROGUE_DEADLOCK_CYCLE_LOG
#define ROGUE_DEADLOCK_CYCLE_LOG 16
#endif

    int rogue_deadlock_register_resource(int resource_id);
    int rogue_deadlock_acquire(int tx_id, int resource_id);
    int rogue_deadlock_release(int tx_id, int resource_id);
    int rogue_deadlock_release_all(int tx_id);
    int rogue_deadlock_tick(uint64_t now_ms);
    int rogue_deadlock_tx_holds(int tx_id, int resource_id);

    typedef struct RogueDeadlockCycle
    {
        uint64_t seq;
        size_t tx_count;
        int tx_ids[16];
        int victim_tx_id;
    } RogueDeadlockCycle;

    typedef struct RogueDeadlockStats
    {
        uint64_t resources_registered;
        uint64_t acquisitions;
        uint64_t waits;
        uint64_t deadlocks_detected;
        uint64_t victims_aborted;
        uint64_t releases;
        uint64_t ticks;
        uint64_t wait_promotions;
    } RogueDeadlockStats;

    void rogue_deadlock_get_stats(RogueDeadlockStats* out);
    int rogue_deadlock_cycles_get(const RogueDeadlockCycle** out_cycles, size_t* count);
    void rogue_deadlock_dump(void* fptr);
    void rogue_deadlock_reset_all(void);
    void rogue_deadlock_set_abort_callback(int (*fn)(int tx_id, const char* reason));
    void rogue_deadlock_on_tx_abort(int tx_id);
    void rogue_deadlock_on_tx_commit(int tx_id);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_DEADLOCK_MANAGER_H
