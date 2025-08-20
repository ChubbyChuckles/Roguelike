/* Crafting Queue & Station Registry (Phase 4.3–4.5)
 * Provides:
 *  - Station registry (fixed set) with capacity lookup
 *  - Craft job queue per station (FIFO waiting, limited parallel active slots)
 *  - Deterministic update advancing time_ms and issuing outputs on completion
 *  - Cancel API with full refund (waiting) or partial refund (active)
 */
#ifndef ROGUE_CRAFTING_QUEUE_H
#define ROGUE_CRAFTING_QUEUE_H

#include "core/crafting.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { ROGUE_CRAFT_STATION_FORGE=0, ROGUE_CRAFT_STATION_ALCHEMY=1, ROGUE_CRAFT_STATION_WORKBENCH=2, ROGUE_CRAFT_STATION_ALTAR=3, ROGUE_CRAFT_STATION__COUNT };

typedef struct RogueCraftJob {
    int id;                /* stable job id */
    int station;           /* station id */
    int recipe_index;      /* index into recipe list */
    int total_ms;          /* total required time */
    int remaining_ms;      /* remaining time */
    int state;             /* 0=waiting,1=active,2=done,3=canceled */
} RogueCraftJob;

/* Map recipe.station string -> station id or -1 */
int rogue_craft_station_id(const char* tag);
int rogue_craft_station_capacity(int station_id);

void rogue_craft_queue_reset(void);
int  rogue_craft_queue_job_count(void);
int  rogue_craft_queue_active_count(int station_id);
const RogueCraftJob* rogue_craft_queue_job_at(int index);

/* Enqueue job: consumes inputs immediately. Returns job id >=0 or <0 error codes:
 *  -1 invalid args, -2 recipe missing, -3 skill requirement unmet, -4 insufficient materials, -5 inventory consume failure */
int rogue_craft_queue_enqueue(const RogueCraftRecipe* recipe, int recipe_index,
    int current_skill,
    RogueInvGetFn inv_get, RogueInvConsumeFn inv_consume);

/* Update all stations by delta_ms; when jobs complete, adds outputs via inv_add. */
void rogue_craft_queue_update(int delta_ms, RogueInvAddFn inv_add);

/* Cancel a job by id. If waiting: full refund. If active: 50% refund (floor) of each input still considered consumed. Returns 0 success, <0 on failure. */
int rogue_craft_queue_cancel(int job_id, const RogueCraftRecipe* recipe, RogueInvAddFn inv_add);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CRAFTING_QUEUE_H */
