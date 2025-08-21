// Shared Memory Pool System (Phase 4.2)
// Provides fixed-block, buddy, and slab allocation strategies with tracking,
// fragmentation metrics, leak detection, and basic optimization suggestions.
// Implemented in C (no C++). Thread-safety optional (enabled when SDL present).

#ifndef ROGUE_MEMORY_POOL_H
#define ROGUE_MEMORY_POOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{ // (Defensive; project is C-only.)
#endif

    // Pool category classification by intended usage pattern / block size.
    typedef enum RoguePoolCategory
    {
        ROGUE_POOL_TINY = 0, // 32 bytes blocks
        ROGUE_POOL_SMALL,    // 64 bytes
        ROGUE_POOL_MEDIUM,   // 128 bytes
        ROGUE_POOL_LARGE,    // 256 bytes
        ROGUE_POOL_XL,       // 512 bytes
        ROGUE_POOL_COUNT
    } RoguePoolCategory;

    // Handle identifying a registered slab class (homogeneous objects with ctor/dtor).
    typedef int32_t RogueSlabHandle; // -1 invalid

    typedef void (*RogueSlabCtor)(void* obj); // optional
    typedef void (*RogueSlabDtor)(void* obj); // optional

    typedef struct RogueMemoryPoolStats
    {
        size_t category_capacity[ROGUE_POOL_COUNT]; // total bytes reserved (all pages)
        size_t category_in_use[ROGUE_POOL_COUNT];   // bytes handed to callers (<= capacity)
        size_t category_allocs[ROGUE_POOL_COUNT];   // successful alloc count
        size_t category_frees[ROGUE_POOL_COUNT];
        // Buddy allocator metrics
        size_t buddy_total_bytes;  // arena size
        size_t buddy_free_bytes;   // total free
        float buddy_fragmentation; // 0..1 (higher = worse)
        // Slab metrics (aggregated)
        size_t slab_classes;          // registered classes
        size_t slab_pages;            // total pages allocated
        size_t slab_objects_live;     // objects currently allocated
        size_t slab_objects_capacity; // total object slots across pages
        // Leak / tracking metrics
        size_t live_allocs; // outstanding generic (category + buddy) allocations
        size_t live_bytes;  // sum of outstanding bytes
        size_t peak_live_bytes;
        size_t alloc_failures; // allocation attempts that failed (OOM or size)
    } RogueMemoryPoolStats;

    typedef struct RogueMemoryPoolRecommendation
    {
        bool advise_expand_tiny;
        bool advise_reduce_xl;
        bool advise_rebalance_buddy;      // fragmentation high
        bool advise_add_slab_page;        // utilization high
        bool advise_enable_thread_safety; // if disabled but contention observed
    } RogueMemoryPoolRecommendation;

    // Initialize system; buddy_arena_bytes must be power-of-two (min 64KB). If 0 uses default 1MB.
    // Returns 0 on success.
    int rogue_memory_pool_init(size_t buddy_arena_bytes, bool thread_safe);
    void
    rogue_memory_pool_shutdown(void); // releases all internal pages/arenas (leak report optional)

    // Generic allocate route chooses strategy:
    //  - size <= 512 -> fixed-block pool (category by size class)
    //  - size > 512 and power-of-two -> buddy allocator
    //  - size > 512 non power-of-two -> rounded up to next power-of-two via buddy
    void* rogue_mp_alloc(size_t size);
    void rogue_mp_free(void* ptr);

    // Category-specific explicit allocate (faster when known at callsite)
    void* rogue_mp_alloc_category(RoguePoolCategory cat);

    // Buddy defragmentation attempt (coalesce pass). Returns number of merges performed.
    int rogue_mp_buddy_defragment(void);

    // Slab API --------------------------------------------------------------
    // Register a slab class for objects of given size (<= 2048). page_obj_count (>=8) controls
    // density.
    RogueSlabHandle rogue_slab_register(const char* name, size_t obj_size, int page_obj_count,
                                        RogueSlabCtor ctor, RogueSlabDtor dtor);
    void* rogue_slab_alloc(RogueSlabHandle handle);
    void rogue_slab_free(RogueSlabHandle handle, void* obj);

    // Shrink slabs: releases fully free pages for all classes. Returns pages freed.
    int rogue_slab_shrink(void);

    // Stats & diagnostics ---------------------------------------------------
    void rogue_memory_pool_get_stats(RogueMemoryPoolStats* out_stats);
    void rogue_memory_pool_get_recommendations(RogueMemoryPoolRecommendation* out_rec);

    // Dump human-readable diagnostics to stdout (or log) – lightweight; safe to call anytime.
    void rogue_memory_pool_dump(void);

    // Validate internal invariants; returns true if ok.
    bool rogue_memory_pool_validate(void);

    // Leak iteration callback: invoked during shutdown dump for each outstanding allocation.
    typedef void (*RogueLeakCallback)(void* ptr, size_t size, const char* origin_tag);
    void rogue_memory_pool_enumerate_leaks(RogueLeakCallback cb);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_MEMORY_POOL_H
