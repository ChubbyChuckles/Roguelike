// Copy-on-Write & Data Sharing (Phase 4.5)
// Chunked page-level copy-on-write buffer supporting incremental cloning and
// per-page duplication on first write, with optional deduplication, stats,
// serialization, and debugging helpers.

#ifndef ROGUE_COW_H
#define ROGUE_COW_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RogueCowBuffer RogueCowBuffer; // opaque

// Statistics for diagnostics & profiling.
typedef struct RogueCowStats {
    uint64_t buffers_created;
    uint64_t pages_created;
    uint64_t cow_triggers;          // first-write duplications
    uint64_t page_copies;           // number of page copy operations
    uint64_t dedup_hits;            // pages replaced by an existing identical page
    uint64_t serialize_linearizations; // full linearization events
} RogueCowStats;

// Create buffer from raw bytes (copied) split into pages of chunk_size (default
// if zero -> 4096). Returns NULL on allocation failure.
RogueCowBuffer *rogue_cow_create_from_bytes(const void *data, size_t len, size_t chunk_size);

// Clone buffer (shares all pages; O(1)).
RogueCowBuffer *rogue_cow_clone(RogueCowBuffer *src);

// Destroy buffer (releases all shared pages; pages freed when last buffer releases them).
void rogue_cow_destroy(RogueCowBuffer *buf);

// Size in bytes.
size_t rogue_cow_size(const RogueCowBuffer *buf);

// Read len bytes at offset into out. Returns 0 on success, -1 out of range.
int rogue_cow_read(const RogueCowBuffer *buf, size_t offset, void *out, size_t len);

// Write len bytes at offset from src. Triggers per-page copy if page shared.
// Returns 0 on success, -1 on OOB or allocation failure.
int rogue_cow_write(RogueCowBuffer *buf, size_t offset, const void *src, size_t len);

// Attempt deduplication: identical page contents within this buffer are unified
// (and optionally across previously deduped global pages). Cheap open-addressing
// table, not guaranteed to remove all duplicates if table fills.
void rogue_cow_dedup(RogueCowBuffer *buf);

// Serialize buffer into flat contiguous bytes (linearize). If out NULL returns
// required size. If max < required, writes what fits and returns required.
size_t rogue_cow_serialize(const RogueCowBuffer *buf, void *out, size_t max);

// Deserialize raw bytes into a new buffer with provided chunk_size (0 -> 4096).
RogueCowBuffer *rogue_cow_deserialize(const void *data, size_t len, size_t chunk_size);

// Stats snapshot.
void rogue_cow_get_stats(RogueCowStats *out);

// Debug dump to FILE* (stdout if f NULL).
void rogue_cow_dump(RogueCowBuffer *buf, void *f /* FILE* */);

// Internal/test helper: return strong refcount of page index (or 0xFFFFFFFF if OOB).
uint32_t rogue_cow_page_refcount(RogueCowBuffer *buf, size_t page_index);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_COW_H
