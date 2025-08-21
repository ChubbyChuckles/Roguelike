// Rollback & Recovery Mechanisms (Phase 5.4)
#ifndef ROGUE_ROLLBACK_MANAGER_H
#define ROGUE_ROLLBACK_MANAGER_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" { 
#endif

// Rollback entry may store full snapshot copy or delta relative to previous full entry.
// For simplicity delta entries only reference the immediate prior FULL entry (no chains of deltas).

typedef struct RogueRollbackStats {
	uint64_t checkpoints_captured;
	uint64_t restores_performed;
	uint64_t validation_failures;
	uint64_t partial_rollbacks;
	uint64_t auto_rollbacks; // via transaction integration
	uint64_t delta_entries;  // number of stored delta entries
	uint64_t bytes_full;     // bytes stored as full copies
	uint64_t bytes_delta;    // bytes stored inside delta payloads
	uint64_t bytes_saved_via_delta; // full_size - delta_size accumulation
	uint64_t systems_rewound; // systems affected by rollback operations
	uint64_t bytes_rewound;   // cumulative bytes of state restored
} RogueRollbackStats;

// Per-operation rollback impact record
typedef struct RogueRollbackEvent {
	uint64_t timestamp;        // monotonic event index
	int system_id;             // system affected
	uint32_t from_version;     // version prior to rollback (best-effort; 0 if unknown)
	uint32_t to_version;       // target version restored
	size_t bytes_rewound;      // size of restored snapshot
	uint8_t was_delta;         // 1 if restored entry was stored as delta
	uint8_t auto_triggered;    // 1 if initiated via auto rollback (transaction abort)
} RogueRollbackEvent;

// Retrieve pointer to internal immutable array of rollback events (chronological oldest->newest).
// Returns 0 on success; out_events set to NULL if no events; count set to number of valid entries.
int rogue_rollback_events_get(const RogueRollbackEvent** out_events, size_t* count);

int rogue_rollback_configure(int system_id, uint32_t capacity); // enable ring for system
int rogue_rollback_capture(int system_id); // capture via snapshot manager + enqueue
int rogue_rollback_capture_multi(const int* system_ids, size_t count);
int rogue_rollback_step_back(int system_id, uint32_t steps); // restore snapshot 'steps' back
int rogue_rollback_partial(const int* system_ids, const uint32_t* steps, size_t count); // per-system step counts
int rogue_rollback_latest(int system_id); // reapply latest snapshot (consistency check)
int rogue_rollback_purge(int system_id); // clear history
int rogue_rollback_dump(void* fptr);
void rogue_rollback_get_stats(RogueRollbackStats* out);
// Auto rollback invoked by transaction abort path for mapped participant.
int rogue_rollback_auto_for_participant(int participant_id);

// Transaction manager integration: map participant -> system snapshot id for auto rollback on abort.
int rogue_rollback_map_participant(int participant_id, int system_id);

#ifdef __cplusplus
}
#endif
#endif
