// Rollback & Recovery Mechanisms (Phase 5.4)
#ifndef ROGUE_ROLLBACK_MANAGER_H
#define ROGUE_ROLLBACK_MANAGER_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" { 
#endif

// Per-system ring buffer storing snapshot generation indices referencing snapshot_manager snapshots.

int rogue_rollback_configure(int system_id, uint32_t capacity); // enable ring for system
int rogue_rollback_capture(int system_id); // capture via snapshot manager + enqueue
int rogue_rollback_step_back(int system_id, uint32_t steps); // restore snapshot 'steps' back
int rogue_rollback_latest(int system_id); // reapply latest snapshot (consistency check)
int rogue_rollback_purge(int system_id); // clear history
int rogue_rollback_dump(void* fptr);

#ifdef __cplusplus
}
#endif
#endif
