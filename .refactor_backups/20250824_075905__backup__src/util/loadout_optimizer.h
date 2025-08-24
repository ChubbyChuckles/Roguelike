/* Loadout Optimization (Equipment Phase 9) */
#ifndef ROGUE_LOADOUT_OPTIMIZER_H
#define ROGUE_LOADOUT_OPTIMIZER_H

#include "../core/equipment/equipment.h"
#include "../core/stat_cache.h"

/* Snapshot of equipped slots for comparison */
typedef struct RogueLoadoutSnapshot
{
    int slot_count;
    int def_indices[ROGUE_EQUIP_SLOT_COUNT];
    int inst_indices[ROGUE_EQUIP_SLOT_COUNT];
    int dps_estimate;
    int ehp_estimate;
    int mobility_index;
} RogueLoadoutSnapshot;

/* Populate snapshot from current equipment & stat cache. Returns 0 on success. */
int rogue_loadout_snapshot(RogueLoadoutSnapshot* out);

/* Compare two snapshots; returns diff count of slot changes and fills arrays with per-slot delta
 * flags (1 changed / 0 same) if provided. */
int rogue_loadout_compare(const RogueLoadoutSnapshot* a, const RogueLoadoutSnapshot* b,
                          int* out_slot_changed);

/* Phase 9.2 heuristic optimizer: given minimum mobility & EHP thresholds, attempts hill-climb to
   raise DPS. Returns number of improvements applied. Non-destructive if no improvement keeps
   constraints. */
int rogue_loadout_optimize(int min_mobility, int min_ehp);

/* Phase 14.4: Asynchronous (background thread) variant. Launches optimization on a worker
    thread so the caller can continue (e.g., main loop). Returns 0 on successful launch,
    negative on failure or if a job is already running. Use join to retrieve the result. */
int rogue_loadout_optimize_async(int min_mobility, int min_ehp);
/* Blocks until async optimization (if any) completes. Returns improvements count or negative
    if no job was running. */
int rogue_loadout_optimize_join(void);
/* Query async state: returns 1 if running, 0 otherwise. */
int rogue_loadout_optimize_async_running(void);

/* Internal: deterministic hash of snapshot (FNV-1a) for caching & pruning (Phase 9.4). */
unsigned int rogue_loadout_hash(const RogueLoadoutSnapshot* s);

/* Cache clear (Phase 9.4): resets evaluated state */
void rogue_loadout_cache_reset(void);

/* Cache statistics (Phase 9.4 test harness): returns number of used entries, capacity, hit count,
 * insert count. */
void rogue_loadout_cache_stats(int* used, int* capacity, int* hits, int* inserts);

#endif
