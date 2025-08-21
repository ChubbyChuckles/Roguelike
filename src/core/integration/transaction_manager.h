// Multi-System Transaction Framework (Phase 5.3)
// Two-phase commit, isolation levels, timeouts, logging, rollback, stats.
#ifndef ROGUE_TRANSACTION_MANAGER_H
#define ROGUE_TRANSACTION_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum RogueTxIsolation
    {
        ROGUE_TX_ISO_READ_COMMITTED = 1,
        ROGUE_TX_ISO_REPEATABLE_READ = 2
    } RogueTxIsolation;

    typedef enum RogueTxState
    {
        ROGUE_TX_STATE_UNUSED = 0,
        ROGUE_TX_STATE_ACTIVE,
        ROGUE_TX_STATE_PREPARING,
        ROGUE_TX_STATE_COMMITTING,
        ROGUE_TX_STATE_COMMITTED,
        ROGUE_TX_STATE_ABORTED,
        ROGUE_TX_STATE_TIMED_OUT
    } RogueTxState;

    // Participant callbacks
    typedef struct RogueTxParticipantDesc
    {
        int participant_id;
        const char* name;
        void* user;
        int (*on_prepare)(void* user, int tx_id, char* errbuf, size_t errcap,
                          uint32_t* out_version);
        int (*on_commit)(void* user, int tx_id);
        int (*on_abort)(void* user, int tx_id);
        uint32_t (*get_version)(void* user); // for isolation read tracking
    } RogueTxParticipantDesc;

    int rogue_tx_register_participant(const RogueTxParticipantDesc* desc);

    typedef struct RogueTxStats
    {
        uint64_t started;
        uint64_t committed;
        uint64_t aborted;
        uint64_t prepare_failures;
        uint64_t isolation_violations;
        uint64_t timeouts;
        uint64_t rollback_invocations;
        uint64_t active_peak;
        uint64_t log_entries;
    } RogueTxStats;

    typedef struct RogueTxLogEntry
    {
        int tx_id;
        RogueTxState from_state;
        RogueTxState to_state;
        uint64_t timestamp_ms;
        RogueTxIsolation isolation;
        uint32_t participants_marked;
    } RogueTxLogEntry;

    // Begin new transaction. Returns tx_id >=0 or negative on error.
    int rogue_tx_begin(RogueTxIsolation isolation, uint32_t timeout_ms);
    // Mark participant involvement.
    int rogue_tx_mark(int tx_id, int participant_id);
    // Read participant version (records baseline for repeatable read isolation).
    int rogue_tx_read(int tx_id, int participant_id, uint32_t* out_version);
    // Commit (runs 2PC). Returns 0 success, <0 on error/abort (still transitions state
    // appropriately).
    int rogue_tx_commit(int tx_id);
    // Explicit abort (can be called by user or internal logic). Idempotent.
    int rogue_tx_abort(int tx_id, const char* reason);
    // Get transaction state.
    RogueTxState rogue_tx_get_state(int tx_id);
    // Advance logical time source (tests) or install custom time function.
    typedef uint64_t (*RogueTxTimeFn)(void);
    void rogue_tx_set_time_source(RogueTxTimeFn fn);
    // Get stats snapshot.
    void rogue_tx_get_stats(RogueTxStats* out);
    // Retrieve log (returns pointer to internal ring, count). Count 0 if disabled.
    int rogue_tx_log_get(const RogueTxLogEntry** out_entries, size_t* out_count);
    // Enable/disable logging (capacity>0 enable; 0 disable & clear).
    int rogue_tx_log_enable(size_t capacity);
    // Dump debug info to FILE* (pass NULL for stdout).
    void rogue_tx_dump(void* fptr);
    // Reset manager (tests) - clears transactions (must not be active in production use).
    void rogue_tx_reset_all(void);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_TRANSACTION_MANAGER_H
