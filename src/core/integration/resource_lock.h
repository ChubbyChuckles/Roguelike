// Resource locking, contention monitoring & deadlock detection (Phase 4.7)
// Provides: mutex & rwlock abstractions with ordering-based deadlock prevention,
// priority-aware acquisition, timeout, stats, auditing & profiling hooks.

#ifndef ROGUE_RESOURCE_LOCK_H
#define ROGUE_RESOURCE_LOCK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" { 
#endif

typedef enum RogueLockPriority {
    ROGUE_LOCK_PRIORITY_BACKGROUND = 0,
    ROGUE_LOCK_PRIORITY_NORMAL = 1,
    ROGUE_LOCK_PRIORITY_CRITICAL = 2,
} RogueLockPriority;

typedef struct RogueMutex RogueMutex;
typedef struct RogueRwLock RogueRwLock;

typedef struct RogueLockStats {
    uint64_t acquisitions;
    uint64_t contended_acquisitions;
    uint64_t failed_timeouts;
    uint64_t failed_deadlocks;
    uint64_t priority_acq[3];
    uint64_t wait_time_ns; // accumulated wait time before successful acquire
} RogueLockStats;

// Create mutex with ordering id (monotonic ascending recommended).
RogueMutex *rogue_mutex_create(int order_id, const char *name);
void rogue_mutex_destroy(RogueMutex *m);
// Acquire with priority & timeout (ms). timeout_ms=0 => non-blocking try, <0 => infinite.
// Returns 0 success; non-zero on timeout/deadlock.
int rogue_mutex_acquire(RogueMutex *m, RogueLockPriority pri, int timeout_ms);
void rogue_mutex_release(RogueMutex *m);
const char *rogue_mutex_name(const RogueMutex *m);
void rogue_mutex_get_stats(const RogueMutex *m, RogueLockStats *out);

// RW lock API (shared readers, exclusive writer). Ordering enforced on write acquire only.
RogueRwLock *rogue_rwlock_create(int order_id, const char *name);
void rogue_rwlock_destroy(RogueRwLock *l);
int rogue_rwlock_acquire_read(RogueRwLock *l, RogueLockPriority pri, int timeout_ms);
int rogue_rwlock_acquire_write(RogueRwLock *l, RogueLockPriority pri, int timeout_ms);
void rogue_rwlock_release_read(RogueRwLock *l);
void rogue_rwlock_release_write(RogueRwLock *l);
void rogue_rwlock_get_stats(const RogueRwLock *l, RogueLockStats *read_out, RogueLockStats *write_out);

// Global stats & auditing
typedef struct RogueGlobalLockStats {
    uint32_t mutex_count;
    uint32_t rwlock_count;
    uint64_t total_acquisitions;
    uint64_t total_contentions;
    uint64_t total_deadlock_preventions;
    uint64_t total_timeouts;
} RogueGlobalLockStats;

void rogue_lock_global_stats(RogueGlobalLockStats *out);
void rogue_lock_dump(void *fptr); // human-readable dump of all locks & stats

// Reset per-lock & global stats (testing)
void rogue_lock_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif
