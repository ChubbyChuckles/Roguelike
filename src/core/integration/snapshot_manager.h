// System State Snapshot & Differential Synchronization (Phase 5.1 / 5.2)
// Lightweight per-system snapshot registry with versioned read-only snapshots,
// differential delta generation & application, caching, hashing, and validation.

#ifndef ROGUE_SNAPSHOT_MANAGER_H
#define ROGUE_SNAPSHOT_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*RogueSnapshotCaptureFn)(void* user, void** out_data, size_t* out_size, uint32_t* out_version);

typedef struct RogueSnapshotDesc {
    int system_id;
    const char* name;
    RogueSnapshotCaptureFn capture;
    void* user;
    size_t max_size; // advisory ceiling; 0 = unlimited
} RogueSnapshotDesc;

typedef struct RogueSystemSnapshot {
    int system_id;
    const char* name;
    uint32_t version;
    uint64_t hash;      // FNV1a 64
    size_t size;
    void* data;         // owned (malloc)
    uint64_t timestamp; // monotonic capture index
} RogueSystemSnapshot;

typedef struct RogueSnapshotDeltaRange { size_t offset; size_t length; } RogueSnapshotDeltaRange;

typedef struct RogueSnapshotDelta {
    int system_id;
    uint32_t base_version;
    uint32_t target_version;
    size_t range_count;
    RogueSnapshotDeltaRange* ranges;
    unsigned char* data; // concatenated changed bytes
    size_t data_size;
} RogueSnapshotDelta;

typedef struct RogueSnapshotStats {
    uint32_t registered_systems;
    uint64_t total_captures;
    uint64_t total_capture_failures;
    uint64_t total_bytes_stored;
    uint64_t total_delta_generated;
    uint64_t total_delta_bytes;
    uint64_t total_delta_applied;
    uint64_t bytes_saved_via_delta;
    uint64_t validation_failures;
} RogueSnapshotStats;

int rogue_snapshot_register(const RogueSnapshotDesc* desc);
int rogue_snapshot_capture(int system_id);
const RogueSystemSnapshot* rogue_snapshot_get(int system_id);
int rogue_snapshot_delta_build(const RogueSystemSnapshot* base, const RogueSystemSnapshot* target, RogueSnapshotDelta* out);
int rogue_snapshot_delta_apply(const RogueSystemSnapshot* base, const RogueSnapshotDelta* delta, void** out_new_data, size_t* out_size, uint64_t* out_hash);
void rogue_snapshot_delta_free(RogueSnapshotDelta* d);
void rogue_snapshot_get_stats(RogueSnapshotStats* out);
void rogue_snapshot_dump(void* fptr);
uint64_t rogue_snapshot_rehash(const RogueSystemSnapshot* snap);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_SNAPSHOT_MANAGER_H
