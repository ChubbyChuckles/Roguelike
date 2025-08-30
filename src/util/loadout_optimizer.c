/**
 * @file loadout_optimizer.c
 * @brief Loadout optimization system using hill-climbing algorithm with caching.
 * @details This module provides equipment optimization functionality that searches
 * for the best item combinations to maximize DPS while maintaining minimum mobility
 * and EHP constraints. Uses caching to avoid re-evaluating identical loadouts.
 */

/* Loadout Optimization (Phase 9) */
#include "loadout_optimizer.h"
#include "../core/app/app_state.h"
#include "../core/equipment/equipment.h"
#include "../core/equipment/equipment_perf.h" /* arena + profiler (Phase 14) */
#include "../core/equipment/equipment_stats.h"
#include "../core/inventory/inventory.h"
#include "../core/loot/loot_instances.h"
#include "../core/loot/loot_item_defs.h"
#include "../game/stat_cache.h"
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

/* Simple FNV-1a 32-bit */

/**
 * @brief Computes FNV-1a hash of arbitrary data.
 * @param data Pointer to the data to hash.
 * @param len Length of the data in bytes.
 * @return 32-bit hash value.
 * @details Uses the FNV-1a algorithm for fast, non-cryptographic hashing.
 */
static unsigned int fnv1a(const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*) data;
    unsigned int h = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

/**
 * @brief Captures a snapshot of the current equipment loadout.
 * @param out Pointer to the snapshot structure to fill.
 * @return 0 on success, -1 on error.
 * @details Records current equipment instances, definition indices, and computed stats.
 */
int rogue_loadout_snapshot(RogueLoadoutSnapshot* out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof *out);
    out->slot_count = ROGUE_EQUIP_SLOT_COUNT;
    for (int i = 0; i < ROGUE_EQUIP_SLOT_COUNT; i++)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) i);
        out->inst_indices[i] = inst;
        if (inst >= 0)
        {
            const RogueItemInstance* it = rogue_item_instance_at(inst);
            out->def_indices[i] = it ? it->def_index : -1;
        }
        else
        {
            out->def_indices[i] = -1;
        }
    }
    out->dps_estimate = g_player_stat_cache.dps_estimate;
    out->ehp_estimate = g_player_stat_cache.ehp_estimate;
    out->mobility_index = g_player_stat_cache.mobility_index;
    return 0;
}

/**
 * @brief Compares two loadout snapshots and identifies differences.
 * @param a First snapshot to compare.
 * @param b Second snapshot to compare.
 * @param out_slot_changed Array to store which slots changed (can be NULL).
 * @return Number of slots that differ between the snapshots.
 * @details Compares equipment definition indices and optionally marks changed slots.
 */
int rogue_loadout_compare(const RogueLoadoutSnapshot* a, const RogueLoadoutSnapshot* b,
                          int* out_slot_changed)
{
    if (!a || !b)
        return -1;
    int diffs = 0;
    int n = (a->slot_count < b->slot_count) ? a->slot_count : b->slot_count;
    for (int i = 0; i < n; i++)
    {
        int changed = (a->def_indices[i] != b->def_indices[i]);
        if (changed)
            diffs++;
        if (out_slot_changed)
            out_slot_changed[i] = changed;
    }
    return diffs;
}

/**
 * @brief Computes a hash of a loadout snapshot for caching purposes.
 * @param s Pointer to the snapshot to hash.
 * @return Hash value of the snapshot.
 * @details Uses FNV-1a to hash equipment definitions and stat estimates.
 */
unsigned int rogue_loadout_hash(const RogueLoadoutSnapshot* s)
{
    if (!s)
        return 0;
    unsigned int h = 0;
    h ^= fnv1a(s->def_indices, sizeof(int) * ROGUE_EQUIP_SLOT_COUNT);
    h = (h << 1) ^ fnv1a(&s->dps_estimate, sizeof(int) * 3);
    return h;
}

/* Evaluated cache (simple open addressing) */

/**
 * @brief Entry in the loadout evaluation cache.
 */
typedef struct CacheEntry
{
    unsigned int hash; /**< Hash of the cached loadout */
    int used;          /**< Whether this cache entry is occupied */
} CacheEntry;

/** @brief Global cache for evaluated loadouts */
static CacheEntry g_cache[256];

/**
 * @brief Resets the loadout evaluation cache.
 * @details Clears all cache entries, resetting the cache to empty state.
 */
void rogue_loadout_cache_reset(void) { memset(g_cache, 0, sizeof g_cache); }

/**
 * @brief Checks if a hash is present in the cache.
 * @param h Hash value to check.
 * @return 1 if hash is in cache, 0 otherwise.
 * @details Uses linear probing to search the cache.
 */
static int cache_contains(unsigned int h)
{
    unsigned int idx = h & 255u;
    for (int i = 0; i < 256; i++)
    {
        unsigned int p = (idx + i) & 255u;
        if (!g_cache[p].used)
            return 0;
        if (g_cache[p].hash == h)
            return 1;
    }
    return 0;
}

/**
 * @brief Inserts a hash into the cache.
 * @param h Hash value to insert.
 * @details Uses linear probing to find an empty slot or existing entry.
 */
static void cache_insert(unsigned int h)
{
    unsigned int idx = h & 255u;
    for (int i = 0; i < 256; i++)
    {
        unsigned int p = (idx + i) & 255u;
        if (!g_cache[p].used)
        {
            g_cache[p].used = 1;
            g_cache[p].hash = h;
            return;
        }
        if (g_cache[p].hash == h)
            return;
    }
}

/** @brief Cache hit counter */
static int g_cache_hits = 0;
/** @brief Cache insert counter */
static int g_cache_inserts = 0;

/**
 * @brief Retrieves cache statistics.
 * @param used Pointer to store number of used cache entries (can be NULL).
 * @param capacity Pointer to store cache capacity (can be NULL).
 * @param hits Pointer to store number of cache hits (can be NULL).
 * @param inserts Pointer to store number of cache inserts (can be NULL).
 * @details Provides statistics about cache usage and performance.
 */
void rogue_loadout_cache_stats(int* used, int* capacity, int* hits, int* inserts)
{
    if (used)
    {
        int c = 0;
        for (int i = 0; i < 256; i++)
            if (g_cache[i].used)
                c++;
        *used = c;
    }
    if (capacity)
        *capacity = 256;
    if (hits)
        *hits = g_cache_hits;
    if (inserts)
        *inserts = g_cache_inserts;
}

/* Helper: recompute stat cache after modifying equipment */

/**
 * @brief Recomputes player stats after equipment changes.
 * @details Forces a complete recalculation of stat cache and bonuses.
 */
static void recompute_stats(void)
{
    extern RoguePlayer g_exposed_player_for_stats;
    rogue_stat_cache_mark_dirty();
    rogue_equipment_apply_stat_bonuses(&g_exposed_player_for_stats);
    rogue_stat_cache_force_update(&g_exposed_player_for_stats);
}

/* Attempt equipping instance (if category fits) returning 0 on success else negative. Wraps
 * existing API. */

/**
 * @brief Attempts to equip an item in a specific slot.
 * @param slot Equipment slot to modify.
 * @param inst_index Item instance index to equip.
 * @param prev_out Pointer to store previous item index (can be NULL).
 * @return 0 on success, negative on failure.
 * @details Wraps the existing equipment API with optional previous item tracking.
 */
static int try_equip_slot(enum RogueEquipSlot slot, int inst_index, int* prev_out)
{
    if (prev_out)
        *prev_out = rogue_equip_get(slot);
    return rogue_equip_try(slot, inst_index);
}

/* Evaluate current equipped DPS/EHP/Mobility after ensuring cache updated */

/**
 * @brief Ensures player stats are up to date.
 * @details Recomputes stats if the cache is marked as dirty.
 */
static void ensure_stats(void)
{
    if (g_player_stat_cache.dirty)
    {
        recompute_stats();
    }
}

/* Return 1 if constraints satisfied, 0 otherwise */

/**
 * @brief Checks if current loadout meets minimum constraints.
 * @param min_mobility Minimum required mobility index.
 * @param min_ehp Minimum required EHP estimate.
 * @return 1 if constraints are satisfied, 0 otherwise.
 * @details Verifies that current stats meet the specified minimums.
 */
static int constraints_ok(int min_mobility, int min_ehp)
{
    ensure_stats();
    if (g_player_stat_cache.mobility_index < min_mobility)
        return 0;
    if (g_player_stat_cache.ehp_estimate < min_ehp)
        return 0;
    return 1;
}

/* Collect candidate item instances by scanning active instances and filtering by category vs slot.
 */

/**
 * @brief Collects candidate items that can be equipped in a slot.
 * @param slot Equipment slot to find candidates for.
 * @param out_indices Array to store candidate instance indices.
 * @param cap Maximum number of candidates to collect.
 * @return Number of candidates collected.
 * @details Scans all item instances and filters by category compatibility with the slot.
 */
static int collect_candidates(enum RogueEquipSlot slot, int* out_indices, int cap)
{
    int count = 0;
    for (int i = 0; i < ROGUE_ITEM_INSTANCE_CAP && count < cap; i++)
    {
        const RogueItemInstance* it = rogue_item_instance_at(i);
        if (!it)
            continue;
        const RogueItemDef* d = rogue_item_def_at(it->def_index);
        if (!d)
            continue;
        int want_cat = 0;
        switch (slot)
        {
        case ROGUE_EQUIP_WEAPON:
            want_cat = ROGUE_ITEM_WEAPON;
            break;
        case ROGUE_EQUIP_OFFHAND:
            want_cat = ROGUE_ITEM_ARMOR;
            break;
        default:
            want_cat = ROGUE_ITEM_ARMOR;
            break;
        }
        if (d->category != want_cat)
            continue;
        out_indices[count++] = i;
    }
    return count;
}

/* Hill-climb: single pass trying improving swaps per slot until no improvement. */

/**
 * @brief Optimizes the current loadout using hill-climbing algorithm.
 * @param min_mobility Minimum required mobility index.
 * @param min_ehp Minimum required EHP estimate.
 * @return Number of equipment changes made.
 * @details Uses hill-climbing to find better equipment combinations while respecting constraints.
 */
int rogue_loadout_optimize(int min_mobility, int min_ehp)
{
    rogue_equip_profiler_zone_begin("optimize");
    ensure_stats();
    RogueLoadoutSnapshot baseline;
    rogue_loadout_snapshot(&baseline);
    unsigned int base_hash = rogue_loadout_hash(&baseline);
    cache_insert(base_hash);
    g_cache_inserts++;
    int improved_total = 0;
    int progress = 1;
    int guard = 0;
    while (progress && guard < 32)
    {
        progress = 0;
        guard++;
        for (int slot = 0; slot < ROGUE_EQUIP_SLOT_COUNT; ++slot)
        {
            int current_inst = rogue_equip_get((enum RogueEquipSlot) slot);
            int* candidates = (int*) rogue_equip_frame_alloc(sizeof(int) * 128, sizeof(int));
            if (!candidates)
            {
                int candidates_stack[128];
                candidates = candidates_stack;
            }
            int ccount = collect_candidates((enum RogueEquipSlot) slot, candidates, 128);
            int best_dps = g_player_stat_cache.dps_estimate;
            int best_inst = current_inst;
            for (int ci = 0; ci < ccount; ++ci)
            {
                int cand = candidates[ci];
                if (cand == current_inst)
                    continue;
                int prev;
                if (try_equip_slot((enum RogueEquipSlot) slot, cand, &prev) == 0)
                {
                    recompute_stats();
                    RogueLoadoutSnapshot snap;
                    rogue_loadout_snapshot(&snap);
                    unsigned int h = rogue_loadout_hash(&snap);
                    if (cache_contains(h))
                    {
                        g_cache_hits++; /* revert */
                        rogue_equip_try((enum RogueEquipSlot) slot, prev);
                        continue;
                    }
                    else
                    {
                        cache_insert(h);
                        g_cache_inserts++;
                    }
                    if (constraints_ok(min_mobility, min_ehp))
                    {
                        if (g_player_stat_cache.dps_estimate > best_dps)
                        {
                            best_dps = g_player_stat_cache.dps_estimate;
                            best_inst = cand;
                        }
                    }
                    rogue_equip_try((enum RogueEquipSlot) slot, prev);
                    recompute_stats();
                }
            }
            if (best_inst != current_inst)
            {
                rogue_equip_try((enum RogueEquipSlot) slot, best_inst);
                recompute_stats();
                improved_total++;
                progress = 1;
            }
        }
    }
    rogue_equip_profiler_zone_end("optimize");
    return improved_total;
}

/* ---------------- Phase 14.4: Async optimization ---------------- */

/** @brief Flag indicating if async optimization is running */
static volatile int g_async_running = 0;
/** @brief Minimum mobility constraint for async optimization */
static int g_async_min_mob = 0;
/** @brief Minimum EHP constraint for async optimization */
static int g_async_min_ehp = 0;
/** @brief Result of async optimization */
static int g_async_result = 0;

#if defined(_WIN32)
/** @brief Handle to the async optimization thread */
static HANDLE g_async_thread = NULL;

/**
 * @brief Thread function for async optimization on Windows.
 * @param p Unused parameter.
 * @return Always 0.
 * @details Runs the optimization and sets the result.
 */
static DWORD WINAPI rogue__opt_thread(LPVOID p)
{
    (void) p;
    g_async_result = rogue_loadout_optimize(g_async_min_mob, g_async_min_ehp);
    g_async_running = 0;
    return 0;
}
#endif

/**
 * @brief Starts asynchronous loadout optimization.
 * @param min_mobility Minimum required mobility index.
 * @param min_ehp Minimum required EHP estimate.
 * @return 0 on success, negative on error.
 * @details Launches optimization in a background thread (Windows) or synchronously (other
 * platforms).
 */
int rogue_loadout_optimize_async(int min_mobility, int min_ehp)
{
    if (g_async_running)
        return -1;
    g_async_running = 1;
    g_async_min_mob = min_mobility;
    g_async_min_ehp = min_ehp;
    g_async_result = 0;
    rogue_equip_profiler_zone_begin("optimize_async_launch");
    rogue_equip_profiler_zone_end("optimize_async_launch");
#if defined(_WIN32)
    g_async_thread = CreateThread(NULL, 0, rogue__opt_thread, NULL, 0, NULL);
    if (!g_async_thread)
    {
        g_async_running = 0;
        return -2;
    }
    return 0;
#else
    /* Fallback: run synchronously (still sets result) */
    g_async_result = rogue_loadout_optimize(min_mobility, min_ehp);
    g_async_running = 0;
    return 0;
#endif
}

/**
 * @brief Waits for asynchronous optimization to complete.
 * @return Number of equipment changes made, or negative on error.
 * @details Blocks until optimization finishes and returns the result.
 */
int rogue_loadout_optimize_join(void)
{
    if (!g_async_running)
    {
#if defined(_WIN32)
        if (g_async_thread)
        {
            WaitForSingleObject(g_async_thread, INFINITE);
            CloseHandle(g_async_thread);
            g_async_thread = NULL;
            return g_async_result;
        }
#endif
        return -1;
    }
#if defined(_WIN32)
    WaitForSingleObject(g_async_thread, INFINITE);
    CloseHandle(g_async_thread);
    g_async_thread = NULL;
    int res = g_async_result;
    g_async_running = 0;
    return res;
#else
    return g_async_result; /* already done in fallback */
#endif
}

/**
 * @brief Checks if asynchronous optimization is currently running.
 * @return 1 if running, 0 otherwise.
 * @details Can be used to poll the status of async optimization.
 */
int rogue_loadout_optimize_async_running(void) { return g_async_running ? 1 : 0; }
