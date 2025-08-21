// Reference Counting & Lifecycle Management (Phase 4.4)
// Generic intrusive reference counted allocation with:
//  - Atomic strong/weak counts
//  - Weak references (upgrade acquire)
//  - Automatic destructor invocation on strong count -> 0
//  - Memory freed when both strong & weak reach 0
//  - Leak tracking & reporting
//  - Live object iteration & DOT graph generation (pluggable edge enumerators)
//  - Persistence snapshot (text) of live objects (id,type,strong,weak)
//  - Lock-free atomic counters; minimal spin lock only for live list mutation

#ifndef ROGUE_REF_COUNT_H
#define ROGUE_REF_COUNT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" { 
#endif

// Allocate a ref-counted object of payload size `size` with destructor `dtor` (may be NULL).
// type_id is user-defined classification for diagnostics & graph enumeration.
// Returns pointer to usable object memory (after internal header).
void *rogue_rc_alloc(size_t size, uint32_t type_id, void (*dtor)(void *));

// Retain / release strong references.
void  rogue_rc_retain(void *obj);
void  rogue_rc_release(void *obj); // invokes dtor when last strong released

// Persistent unique id assigned at allocation (monotonic 64-bit).
uint64_t rogue_rc_get_id(const void *obj);
uint32_t rogue_rc_get_type(const void *obj);
uint32_t rogue_rc_get_strong(const void *obj);
uint32_t rogue_rc_get_weak(const void *obj);

// Weak reference handle (opaque to callers except for creation/acquire/release).
typedef struct RogueWeakRef { struct RogueRcHeader *hdr; } RogueWeakRef;

// Create weak ref from strong object (increment weak count). Returns handle with hdr==NULL if obj NULL.
RogueWeakRef rogue_rc_weak_from(void *obj);
// Acquire strong ref from weak (returns NULL if object already destroyed). On success returns strong pointer (retained).
void *rogue_rc_weak_acquire(RogueWeakRef weak);
// Release weak handle (drops weak count; if reaches 0 and no strong left frees memory).
void  rogue_rc_weak_release(RogueWeakRef *weak);

// Leak tracking / stats.
typedef struct RogueRcStats {
    uint64_t total_allocs;
    uint64_t total_frees;
    uint64_t live_objects;   // strong>0
    uint64_t peak_live;      // high water mark of live strong objects
} RogueRcStats;

void rogue_rc_get_stats(RogueRcStats *out);
// Writes human-readable leak report (objects with strong>0) to FILE* (stdout if f==NULL).
void rogue_rc_dump_leaks(void *f /* FILE* */);

// Live object iteration (strong objects only). Callback returns false to stop.
typedef bool (*RogueRcIterFn)(void *obj, uint32_t type_id, uint64_t id, void *ud);
void rogue_rc_iterate(RogueRcIterFn fn, void *ud);

// Edge enumerator registration for graph visualization.
// enumerate(obj, out_array, max) should write up to max child strong object pointers into out_array, returning count.
typedef size_t (*RogueRcEdgeEnumFn)(void *obj, void **out_children, size_t max);
bool rogue_rc_register_edge_enumerator(uint32_t type_id, RogueRcEdgeEnumFn fn);

// Generate DOT graph of current strong object graph into buffer.
// Returns number of bytes written (truncated if buffer too small). If buf NULL returns required size.
size_t rogue_rc_generate_dot(char *buf, size_t max);

// Persistence snapshot: writes lines "id type strong weak" for each live strong object.
// Returns bytes written (or required size if buf NULL).
size_t rogue_rc_snapshot(char *buf, size_t max);

// Validation helper: returns true if internal invariants hold (counts consistent, etc.).
bool rogue_rc_validate(void);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_REF_COUNT_H
