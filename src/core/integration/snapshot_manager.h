// System State Snapshot & Differential Synchronization (Phase 5.1 / 5.2)
// Lightweight per-system snapshot registry with versioned read-only snapshots,
// differential delta generation & application, caching, hashing, and validation.

#ifndef ROGUE_SNAPSHOT_MANAGER_H
#define ROGUE_SNAPSHOT_MANAGER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef int (*RogueSnapshotCaptureFn)(void* user, void** out_data, size_t* out_size,
                                          uint32_t* out_version);

    typedef struct RogueSnapshotDesc
    {
        int system_id;
        const char* name;
        RogueSnapshotCaptureFn capture;
        void* user;
        size_t max_size; // advisory ceiling; 0 = unlimited
        int (*restore)(void* user, const void* data, size_t size,
                       uint32_t version); // optional restore hook (for rollback)
    } RogueSnapshotDesc;

    typedef struct RogueSystemSnapshot
    {
        int system_id;
        const char* name;
        uint32_t version;
        uint64_t hash; // FNV1a 64
        size_t size;
        void* data;         // owned (malloc)
        uint64_t timestamp; // monotonic capture index
    } RogueSystemSnapshot;

    typedef struct RogueSnapshotDeltaRange
    {
        size_t offset;
        size_t length;
    } RogueSnapshotDeltaRange;

    typedef struct RogueSnapshotDelta
    {
        int system_id;
        uint32_t base_version;
        uint32_t target_version;
        size_t range_count;
        RogueSnapshotDeltaRange* ranges;
        unsigned char* data; // concatenated changed bytes
        size_t data_size;
    } RogueSnapshotDelta;

    typedef struct RogueSnapshotStats
    {
        uint32_t registered_systems;
        uint64_t total_captures;
        uint64_t total_capture_failures;
        uint64_t total_bytes_stored;
        uint64_t total_delta_generated;
        uint64_t total_delta_bytes;
        uint64_t total_delta_applied;
        uint64_t bytes_saved_via_delta;
        uint64_t validation_failures;
        uint64_t delta_validation_failures;
        uint64_t delta_apply_failures;
        uint64_t total_delta_build_ns;
        uint64_t total_delta_apply_ns;
    } RogueSnapshotStats;

    int rogue_snapshot_register(const RogueSnapshotDesc* desc);
    int rogue_snapshot_capture(int system_id);
    const RogueSystemSnapshot* rogue_snapshot_get(int system_id);
    int rogue_snapshot_delta_build(const RogueSystemSnapshot* base,
                                   const RogueSystemSnapshot* target, RogueSnapshotDelta* out);
    int rogue_snapshot_delta_apply(const RogueSystemSnapshot* base, const RogueSnapshotDelta* delta,
                                   void** out_new_data, size_t* out_size, uint64_t* out_hash);
    void rogue_snapshot_delta_free(RogueSnapshotDelta* d);
    void rogue_snapshot_get_stats(RogueSnapshotStats* out);
    void rogue_snapshot_dump(void* fptr);
    uint64_t rogue_snapshot_rehash(const RogueSystemSnapshot* snap);
    // Apply snapshot back to live system via registered restore hook (if provided)
    int rogue_snapshot_restore(int system_id, const RogueSystemSnapshot* snap);

    // ---- Dependency Ordering (5.2.4) ----
    // Declare that 'system_id' requires snapshot/delta of 'depends_on' applied first.
    int rogue_snapshot_dependency_add(int system_id, int depends_on);
    // Produce a topologically sorted order of system IDs respecting dependencies.
    // *inout_count provides capacity on input, actual count on success. Returns 0 on success, <0 on
    // error (cycle detected -2, capacity -3).
    int rogue_snapshot_plan_order(int* out_ids, size_t* inout_count);

    // ---- Delta Replay & Logging (5.2.6) ----
    typedef struct RogueSnapshotDeltaRecord
    {
        int system_id;
        uint32_t base_version;
        uint32_t target_version;
        uint64_t timestamp; // capture timestamp of target snapshot
        size_t full_size;
        size_t delta_size;
        size_t range_count;
        uint64_t target_hash;
    } RogueSnapshotDeltaRecord;

    // Enable in-memory circular replay log with given capacity; passing 0 disables logging.
    int rogue_snapshot_replay_log_enable(size_t capacity);
    // Get pointer to immutable array of records in chronological order (oldest→newest). count
    // returns number of valid records.
    int rogue_snapshot_replay_log_get(const RogueSnapshotDeltaRecord** out_records,
                                      size_t* out_count);
    // Reapply recorded deltas from start_index (0 oldest) count entries to advance system states;
    // validates hashes.
    int rogue_snapshot_replay_apply(size_t start_index, size_t count);

    // Testing / maintenance helpers
    int rogue_snapshot_reset(int system_id); // clear stored snapshot (version=0)

#ifdef __cplusplus
}
#endif

#endif // ROGUE_SNAPSHOT_MANAGER_H
