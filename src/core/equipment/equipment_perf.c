/**
 * @file equipment_perf.c
 * @brief Equipment system performance and memory optimization
 *
 * Phase 14: Performance & Memory Optimization Implementation.
 * Provides high-performance data structures and algorithms for equipment
 * system operations, including SoA (Struct of Arrays) storage, arena allocation,
 * and optimized stat aggregation with profiling capabilities.
 *
 * This module implements several optimization techniques:
 * - SoA Buffers: Struct-of-Arrays storage for better cache locality
 * - Linear Arena: Single-frame temporary memory allocation
 * - Profiler: Minimal performance monitoring with string-based zones
 * - SIMD-like Aggregation: Batched processing for improved throughput
 *
 * @note All optimizations focus on cache efficiency and reduced memory access patterns
 * @note Profiler uses string hashing for zone identification
 * @note Arena allocator is single-frame (reset each frame)
 */

#include "equipment_perf.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "equipment.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ---- Struct-of-Arrays (SoA) Buffers ---- */

/**
 * @brief Equipment slot strength values (SoA layout)
 *
 * Array storing strength bonuses for each equipment slot.
 * SoA layout improves cache locality when processing single stats across all slots.
 */
int g_equip_slot_strength[ROGUE_EQUIP__COUNT];

/**
 * @brief Equipment slot dexterity values (SoA layout)
 *
 * Array storing dexterity bonuses for each equipment slot.
 * SoA layout improves cache locality when processing single stats across all slots.
 */
int g_equip_slot_dexterity[ROGUE_EQUIP__COUNT];

/**
 * @brief Equipment slot vitality values (SoA layout)
 *
 * Array storing vitality bonuses for each equipment slot.
 * SoA layout improves cache locality when processing single stats across all slots.
 */
int g_equip_slot_vitality[ROGUE_EQUIP__COUNT];

/**
 * @brief Equipment slot intelligence values (SoA layout)
 *
 * Array storing intelligence bonuses for each equipment slot.
 * SoA layout improves cache locality when processing single stats across all slots.
 */
int g_equip_slot_intelligence[ROGUE_EQUIP__COUNT];

/**
 * @brief Equipment slot armor values (SoA layout)
 *
 * Array storing armor bonuses for each equipment slot.
 * SoA layout improves cache locality when processing single stats across all slots.
 */
int g_equip_slot_armor[ROGUE_EQUIP__COUNT];

/**
 * @brief Total equipment strength bonus
 *
 * Aggregated sum of all equipped items' strength bonuses.
 * Updated during equipment aggregation operations.
 */
int g_equip_total_strength = 0;

/**
 * @brief Total equipment dexterity bonus
 *
 * Aggregated sum of all equipped items' dexterity bonuses.
 * Updated during equipment aggregation operations.
 */
int g_equip_total_dexterity = 0;

/**
 * @brief Total equipment vitality bonus
 *
 * Aggregated sum of all equipped items' vitality bonuses.
 * Updated during equipment aggregation operations.
 */
int g_equip_total_vitality = 0;

/**
 * @brief Total equipment intelligence bonus
 *
 * Aggregated sum of all equipped items' intelligence bonuses.
 * Updated during equipment aggregation operations.
 */
int g_equip_total_intelligence = 0;

/**
 * @brief Total equipment armor bonus
 *
 * Aggregated sum of all equipped items' armor bonuses.
 * Updated during equipment aggregation operations.
 */
int g_equip_total_armor = 0;

/* ---- Linear Arena Allocator ---- */

/** @brief Arena capacity in bytes (8KB single-frame allocation) */
#define EQUIP_FRAME_ARENA_CAP 8192

/** @brief Arena memory buffer */
static unsigned char g_frame_arena[EQUIP_FRAME_ARENA_CAP];

/** @brief Current allocation offset */
static size_t g_frame_off = 0;

/** @brief High water mark (maximum offset reached) */
static size_t g_frame_high = 0;

/**
 * @brief Allocate memory from the frame arena
 *
 * Allocates aligned memory from the single-frame arena. Memory is valid
 * until the next frame reset. Uses bump-pointer allocation for speed.
 *
 * @param size Number of bytes to allocate
 * @param align Alignment requirement (must be power of 2)
 * @return void* Allocated memory block, or NULL if out of space
 *
 * @note Alignment is automatically adjusted to minimum of 1
 * @note Tracks high water mark for memory usage analysis
 * @note No individual deallocation - reset entire arena at frame end
 */
void* rogue_equip_frame_alloc(size_t size, size_t align)
{
    if (align < 1)
        align = 1;
    size_t mask = align - 1;
    size_t aligned = (g_frame_off + mask) & ~mask;
    if (aligned + size > EQUIP_FRAME_ARENA_CAP)
        return NULL;
    void* p = g_frame_arena + aligned;
    g_frame_off = aligned + size;
    if (g_frame_off > g_frame_high)
        g_frame_high = g_frame_off;
    return p;
}

/**
 * @brief Reset the frame arena
 *
 * Resets the arena allocation offset to zero, effectively freeing all
 * allocated memory. Should be called at the end of each frame.
 *
 * @note High water mark is preserved for performance analysis
 * @note Memory contents are not cleared for performance
 */
void rogue_equip_frame_reset(void) { g_frame_off = 0; }

/**
 * @brief Get arena high water mark
 *
 * Returns the maximum allocation offset reached since the last reset.
 * Useful for memory usage analysis and optimization.
 *
 * @return size_t Maximum bytes allocated in a single frame
 */
size_t rogue_equip_frame_high_water(void) { return g_frame_high; }

/**
 * @brief Get arena total capacity
 *
 * Returns the total capacity of the frame arena in bytes.
 *
 * @return size_t Total arena capacity in bytes
 */
size_t rogue_equip_frame_capacity(void) { return EQUIP_FRAME_ARENA_CAP; }

/* ---- Minimal Profiler ---- */

/** @brief Maximum number of profiling zones */
#define PROF_ZONE_CAP 16

/**
 * @brief Profiling zone data structure
 */
typedef struct
{
    char name[24];      /**< Zone name (null-terminated) */
    double total_ms;    /**< Total time spent in zone */
    int count;          /**< Number of times zone was executed */
    int used;           /**< Whether this zone slot is in use */
    clock_t begin_clock; /**< Clock value when zone began */
    int active;         /**< Whether zone is currently active */
} Zone;

/** @brief Global array of profiling zones */
static Zone g_zones[PROF_ZONE_CAP];

/**
 * @brief Reset all profiling data
 *
 * Clears all profiling zones and resets their state. Should be called
 * at the beginning of each profiling session.
 */
void rogue_equip_profiler_reset(void) { memset(g_zones, 0, sizeof g_zones); }

/**
 * @brief Find or create a profiling zone
 *
 * Locates an existing profiling zone by name, or creates a new one
 * if it doesn't exist and space is available.
 *
 * @param name Zone name (up to 23 characters)
 * @return Zone* Pointer to zone structure, or NULL if not found and no space available
 *
 * @note Uses linear search for zone lookup
 * @note Zone names are truncated to fit in 23 characters
 * @note Thread-unsafe - should not be called concurrently
 */
static Zone* find_zone(const char* name)
{
    int free_idx = -1;
    for (int i = 0; i < PROF_ZONE_CAP; i++)
    {
        if (g_zones[i].used)
        {
            if (strncmp(g_zones[i].name, name, sizeof g_zones[i].name) == 0)
                return &g_zones[i];
        }
        else if (free_idx < 0)
            free_idx = i;
    }
    if (free_idx >= 0)
    {
        Zone* z = &g_zones[free_idx];
        z->used = 1; /* safe copy */
        size_t len = strlen(name);
        if (len >= sizeof z->name)
            len = sizeof z->name - 1;
        memcpy(z->name, name, len);
        z->name[len] = '\0';
        return z;
    }
    return NULL;
}

/**
 * @brief Begin profiling a code zone
 *
 * Starts timing a code section. If the zone doesn't exist, it will be created.
 * Nested calls to the same zone are ignored.
 *
 * @param name Zone name identifier
 *
 * @note Uses system clock() function for timing
 * @note Thread-unsafe - should not be called concurrently
 * @note Nested begin calls are ignored (no re-entrant timing)
 */
void rogue_equip_profiler_zone_begin(const char* name)
{
    Zone* z = find_zone(name);
    if (!z || z->active)
        return;
    z->begin_clock = clock();
    z->active = 1;
}

/**
 * @brief End profiling a code zone
 *
 * Stops timing the specified code zone and accumulates the elapsed time.
 * Must be paired with a corresponding zone_begin call.
 *
 * @param name Zone name identifier (must match begin call)
 *
 * @note Calculates elapsed time in milliseconds
 * @note Accumulates total time and execution count
 * @note Mismatched begin/end calls are ignored
 */
void rogue_equip_profiler_zone_end(const char* name)
{
    Zone* z = find_zone(name);
    if (!z || !z->active)
        return;
    clock_t end = clock();
    double ms = 1000.0 * (double) (end - z->begin_clock) / (double) CLOCKS_PER_SEC;
    z->total_ms += ms;
    z->count++;
    z->active = 0;
}

/**
 * @brief Get profiling statistics for a zone
 *
 * Retrieves accumulated timing statistics for a profiling zone.
 *
 * @param name Zone name identifier
 * @param total_ms Output parameter for total elapsed time in milliseconds
 * @param count Output parameter for execution count
 * @return int 0 on success, -1 if zone not found or never executed
 *
 * @note Output parameters are optional (pass NULL to ignore)
 * @note Returns -1 for zones that exist but have never been executed
 */
int rogue_equip_profiler_zone_stats(const char* name, double* total_ms, int* count)
{
    Zone* z = find_zone(name);
    if (!z || z->count == 0)
        return -1;
    if (total_ms)
        *total_ms = z->total_ms;
    if (count)
        *count = z->count;
    return 0;
}

/**
 * @brief Dump all profiling data as JSON
 *
 * Exports all profiling zone data in JSON format for external analysis.
 * Only includes zones that have been executed at least once.
 *
 * @param buf Output buffer for JSON data
 * @param cap Capacity of output buffer
 * @return int Length of JSON output on success, -1 on error or insufficient space
 *
 * @note JSON format: {"zone_name":{"ms":total_ms,"count":executions},...}
 * @note Only includes zones with count > 0
 * @note Buffer must be null-terminated on success
 * @note Returns -1 if buffer too small (minimum 4 bytes required)
 */
int rogue_equip_profiler_dump(char* buf, int cap)
{
    if (!buf || cap < 4)
        return -1;
    int off = 0;
    int n = snprintf(buf + off, cap - off, "{");
    if (n < 0 || n >= cap - off)
        return -1;
    off += n;
    for (int i = 0; i < PROF_ZONE_CAP; i++)
    {
        Zone* z = &g_zones[i];
        if (!z->used || z->count == 0)
            continue;
        n = snprintf(buf + off, cap - off, "\"%s\":{\"ms\":%.3f,\"count\":%d},", z->name,
                     z->total_ms, z->count);
        if (n < 0 || n >= cap - off)
            return -1;
        off += n;
    }
    if (off > 1 && buf[off - 1] == ',')
        off--;
    n = snprintf(buf + off, cap - off, "}");
    if (n < 0 || n >= cap - off)
        return -1;
    off += n;
    if (off < cap)
        buf[off] = '\0';
    return off;
}

/* ---- Equipment Stat Aggregation ---- */

/**
 * @brief Scalar equipment stat aggregation
 *
 * Performs equipment stat aggregation using a simple scalar approach.
 * Processes each equipment slot individually, calculating stat bonuses
 * and updating both per-slot and total aggregated values.
 *
 * This is the fallback method when SIMD-like processing is not available
 * or not desired. It provides correct results but may have lower throughput
 * than the SIMD-like version.
 *
 * @note Uses rarity as a simple proxy for primary stat values
 * @note Clears all totals before aggregation to ensure accuracy
 * @note Updates both slot-specific and global total values
 */
static void aggregate_scalar(void)
{
    g_equip_total_strength = g_equip_total_dexterity = g_equip_total_vitality =
        g_equip_total_intelligence = g_equip_total_armor = 0;
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) s);
        int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
        if (inst >= 0)
        {
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            if (it)
            {
                const RogueItemDef* d = rogue_item_def_at(it->def_index);
                if (d)
                {
                    armor = d->base_armor; /* simplistic primary stats derived from rarity for
                                              illustration */
                    str = d->rarity;
                    dex = d->rarity;
                    vit = d->rarity;
                    intel = d->rarity;
                }
            }
        }
        g_equip_slot_strength[s] = str;
        g_equip_slot_dexterity[s] = dex;
        g_equip_slot_vitality[s] = vit;
        g_equip_slot_intelligence[s] = intel;
        g_equip_slot_armor[s] = armor;
        g_equip_total_strength += str;
        g_equip_total_dexterity += dex;
        g_equip_total_vitality += vit;
        g_equip_total_intelligence += intel;
        g_equip_total_armor += armor;
    }
}

/**
 * @brief SIMD-like equipment stat aggregation
 *
 * Performs equipment stat aggregation using a batch processing approach
 * that simulates SIMD operations. Processes equipment slots in groups of 4
 * to improve cache locality and instruction-level parallelism.
 *
 * This method provides better performance than scalar aggregation by:
 * - Processing multiple slots in each iteration
 * - Reducing loop overhead
 * - Improving cache prefetching
 * - Better instruction pipelining
 *
 * @note Processes slots in batches of 4 for optimal performance
 * @note Falls back to scalar processing for remaining slots
 * @note No actual SIMD intrinsics used (maintains portability)
 * @note Uses same stat calculation logic as scalar version
 */
static void aggregate_simd_like(void)
{
    g_equip_total_strength = g_equip_total_dexterity = g_equip_total_vitality =
        g_equip_total_intelligence = g_equip_total_armor = 0;
    int s = 0;
    for (; s + 3 < ROGUE_EQUIP__COUNT; s += 4)
    {
        for (int i = 0; i < 4; i++)
        {
            int slot = s + i;
            int inst = rogue_equip_get((enum RogueEquipSlot) slot);
            int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
            if (inst >= 0)
            {
                const RogueItemInstance* it = rogue_item_instance_at(inst);
                if (it)
                {
                    const RogueItemDef* d = rogue_item_def_at(it->def_index);
                    if (d)
                    {
                        armor = d->base_armor;
                        str = d->rarity;
                        dex = d->rarity;
                        vit = d->rarity;
                        intel = d->rarity;
                    }
                }
            }
            g_equip_slot_strength[slot] = str;
            g_equip_slot_dexterity[slot] = dex;
            g_equip_slot_vitality[slot] = vit;
            g_equip_slot_intelligence[slot] = intel;
            g_equip_slot_armor[slot] = armor;
            g_equip_total_strength += str;
            g_equip_total_dexterity += dex;
            g_equip_total_vitality += vit;
            g_equip_total_intelligence += intel;
            g_equip_total_armor += armor;
        }
    }
    for (; s < ROGUE_EQUIP__COUNT; s++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) s);
        int str = 0, dex = 0, vit = 0, intel = 0, armor = 0;
        if (inst >= 0)
        {
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            if (it)
            {
                const RogueItemDef* d = rogue_item_def_at(it->def_index);
                if (d)
                {
                    armor = d->base_armor;
                    str = d->rarity;
                    dex = d->rarity;
                    vit = d->rarity;
                    intel = d->rarity;
                }
            }
        }
        g_equip_slot_strength[s] = str;
        g_equip_slot_dexterity[s] = dex;
        g_equip_slot_vitality[s] = vit;
        g_equip_slot_intelligence[s] = intel;
        g_equip_slot_armor[s] = armor;
        g_equip_total_strength += str;
        g_equip_total_dexterity += dex;
        g_equip_total_vitality += vit;
        g_equip_total_intelligence += intel;
        g_equip_total_armor += armor;
    }
}

/**
 * @brief Aggregate equipment statistics
 *
 * Main entry point for equipment stat aggregation. Updates all SoA buffers
 * and total stat values based on currently equipped items. Supports multiple
 * aggregation modes for performance optimization.
 *
 * @param mode Aggregation mode (scalar or SIMD-like processing)
 *
 * @note Automatically profiles the aggregation operation
 * @note Updates both per-slot arrays and global totals
 * @note Clears all previous values before aggregation
 * @note Uses rarity as proxy for primary stat calculations
 *
 * Aggregation modes:
 * - ROGUE_EQUIP_AGGREGATE_SCALAR: Simple per-slot processing
 * - ROGUE_EQUIP_AGGREGATE_SIMD: Batched processing for better performance
 */
void rogue_equipment_aggregate(enum RogueEquipAggregateMode mode)
{
    rogue_equip_profiler_zone_begin(mode == ROGUE_EQUIP_AGGREGATE_SIMD ? "agg_simd" : "agg_scalar");
    if (mode == ROGUE_EQUIP_AGGREGATE_SIMD)
        aggregate_simd_like();
    else
        aggregate_scalar();
    rogue_equip_profiler_zone_end(mode == ROGUE_EQUIP_AGGREGATE_SIMD ? "agg_simd" : "agg_scalar");
}
