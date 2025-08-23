#/**
 * @file crafting_queue.c
 * @brief Crafting Queue Implementation (Phase 4.3–4.5)
 *
 * Implements a simple crafting job queue used by the game's crafting systems.
 * Jobs are enqueued with material checks, may wait for station capacity,
 * advance over time, and produce outputs when completed. This module
 * interacts with inventory callbacks and crafting skill/perk helpers.
 */
#include "core/crafting/crafting_queue.h"
#include "core/crafting/crafting_skill.h"
#include <string.h>

#ifndef ROGUE_CRAFT_JOB_CAP
/**
 * @brief Maximum number of concurrent jobs stored in the crafting queue.
 *
 * Can be overridden at compile time. Kept as a preprocessor define so
 * other modules can size arrays/allocations conservatively.
 */
#define ROGUE_CRAFT_JOB_CAP 256
#endif

/**
 * @brief Backing storage for craft jobs.
 *
 * An array of RogueCraftJob entries used as a compact queue. Jobs are stored
 * in submission order. The public accessor `rogue_craft_queue_job_at` returns
 * pointers into this array.
 */
static RogueCraftJob g_jobs[ROGUE_CRAFT_JOB_CAP];

/** @brief Number of valid entries currently in @c g_jobs. */
static int g_job_count = 0;

/**
 * @brief Next job id to hand out.
 *
 * Starts at 1; value 0 is reserved to mean "invalid".
 */
static int g_next_id = 1; /* 0 reserved */

/** @brief Base active slot capacities for each crafting station. */
static int station_caps[ROGUE_CRAFT_STATION__COUNT] = {2, 2, 2, 1}; /* base capacities */

/** @brief Maximum number of waiting jobs allowed per station. */
static int station_wait_cap[ROGUE_CRAFT_STATION__COUNT] = {32, 32, 32,
                                                           32}; /* max waiting jobs per station */

/**
 * @brief Convert a textual station tag to its station id.
 *
 * Recognized tags (case-sensitive):
 *  - "forge" -> @c ROGUE_CRAFT_STATION_FORGE
 *  - "alchemy_table", "alchemy" -> @c ROGUE_CRAFT_STATION_ALCHEMY
 *  - "workbench" -> @c ROGUE_CRAFT_STATION_WORKBENCH
 *  - "mystic_altar", "altar" -> @c ROGUE_CRAFT_STATION_ALTAR
 *
 * @param tag Null-terminated string tag for the station. If NULL or
 *            unrecognized the function returns -1.
 * @return station id on success, or -1 on error/unrecognized tag.
 */
int rogue_craft_station_id(const char* tag)
{
    if (!tag)
        return -1;
    if (strcmp(tag, "forge") == 0)
        return ROGUE_CRAFT_STATION_FORGE;
    if (strcmp(tag, "alchemy_table") == 0 || strcmp(tag, "alchemy") == 0)
        return ROGUE_CRAFT_STATION_ALCHEMY;
    if (strcmp(tag, "workbench") == 0)
        return ROGUE_CRAFT_STATION_WORKBENCH;
    if (strcmp(tag, "mystic_altar") == 0 || strcmp(tag, "altar") == 0)
        return ROGUE_CRAFT_STATION_ALTAR;
    return -1;
}
/**
 * @brief Query the active slot capacity for a crafting station.
 *
 * The capacity is the number of jobs that can be actively processed
 * concurrently at the given station.
 *
 * @param station_id Station id from the @c RogueCraftStation enum.
 * @return capacity (>= 0). Returns 0 for invalid station ids.
 */
int rogue_craft_station_capacity(int station_id)
{
    if (station_id < 0 || station_id >= ROGUE_CRAFT_STATION__COUNT)
        return 0;
    return station_caps[station_id];
}

/**
 * @brief Reset the crafting queue to an empty state.
 *
 * This will drop all pending/active jobs and reset job id allocation.
 */
void rogue_craft_queue_reset(void)
{
    g_job_count = 0;
    g_next_id = 1;
}
int rogue_craft_queue_job_count(void) { return g_job_count; }
/**
 * @brief Count active (processing) jobs for a station.
 *
 * A job is considered active when its @c state == 1.
 *
 * @param station_id Station id to query.
 * @return Number of active jobs for the station.
 */
int rogue_craft_queue_active_count(int station_id)
{
    int c = 0;
    for (int i = 0; i < g_job_count; i++)
    {
        if (g_jobs[i].station == station_id && g_jobs[i].state == 1)
            c++;
    }
    return c;
}
/**
 * @brief Get a pointer to the job at the given queue index.
 *
 * The returned pointer references internal storage. Callers must not
 * mutate the entry directly unless the module's API allows it.
 *
 * @param index Zero-based position in the submission-order queue.
 * @return Pointer to the job, or NULL if the index is out of range.
 */
const RogueCraftJob* rogue_craft_queue_job_at(int index)
{
    if (index < 0 || index >= g_job_count)
        return NULL;
    return &g_jobs[index];
}

/**
 * @brief Try to promote waiting jobs to active for all stations.
 *
 * Scans each station and, if it has spare capacity, activates the earliest
 * submitted waiting jobs (state == 0) until the station's active capacity
 * is reached.
 */
static void try_activate_waiting(void)
{
    for (int s = 0; s < ROGUE_CRAFT_STATION__COUNT; s++)
    {
        int cap = rogue_craft_station_capacity(s);
        int active = rogue_craft_queue_active_count(s);
        if (active >= cap)
            continue;
        /* activate earliest waiting jobs in submission order */
        for (int i = 0; i < g_job_count && active < cap; i++)
        {
            if (g_jobs[i].station == s && g_jobs[i].state == 0)
            {
                g_jobs[i].state = 1;
                active++;
            }
        }
    }
}

/**
 * @brief Enqueue a crafting job for the specified recipe.
 *
 * This performs several checks before creating a job:
 *  - Validates arguments
 *  - Verifies the recipe has a valid output
 *  - Checks the player's current skill meets the recipe's requirement
 *  - Ensures required materials exist and consumes them via callbacks
 *  - Respects per-station waiting queue limits and overall job capacity
 *
 * On success a new job entry is appended and its id is returned. The job
 * will initially be in the waiting state (state == 0) and may be promoted
 * to active depending on station capacity.
 *
 * Error codes (negative):
 *  - -1 : invalid arguments
 *  - -2 : recipe has no output
 *  - -3 : insufficient skill
 *  - -4 : missing materials
 *  - -5 : failed to consume materials
 *  - -6 : job queue full
 *  - -7 : station waiting queue full
 *
 * @param recipe Pointer to the recipe definition.
 * @param recipe_index Index of the recipe in the global recipe table.
 * @param current_skill Player's current crafting skill level for the discipline.
 * @param inv_get Callback to query inventory counts for a definition index.
 * @param inv_consume Callback to consume materials from inventory; should
 *                    return the actual amount consumed.
 * @return New job id (>0) on success, or negative error code on failure.
 */
int rogue_craft_queue_enqueue(const RogueCraftRecipe* recipe, int recipe_index, int current_skill,
                              RogueInvGetFn inv_get, RogueInvConsumeFn inv_consume)
{
    if (!recipe || recipe_index < 0 || !inv_get || !inv_consume)
        return -1;
    if (recipe->output_def < 0)
        return -2;
    if (current_skill < recipe->skill_req)
        return -3;
    /* check materials */
    enum RogueCraftDiscipline disc =
        rogue_craft_station_discipline(rogue_craft_station_id(recipe->station));
    int cost_pct = rogue_craft_perk_material_cost_pct(disc);
    if (cost_pct < 1)
        cost_pct = 100;
    /* check materials with cost reduction */
    for (int i = 0; i < recipe->input_count; i++)
    {
        const RogueCraftIngredient* ing = &recipe->inputs[i];
        int need = (ing->quantity * cost_pct + 99) / 100;
        if (need < 1)
            need = 1;
        if (inv_get(ing->def_index) < need)
            return -4;
    }
    for (int i = 0; i < recipe->input_count; i++)
    {
        const RogueCraftIngredient* ing = &recipe->inputs[i];
        int need = (ing->quantity * cost_pct + 99) / 100;
        if (need < 1)
            need = 1;
        if (inv_consume(ing->def_index, need) < need)
            return -5;
    }
    if (g_job_count >= ROGUE_CRAFT_JOB_CAP)
        return -6;
    int station = rogue_craft_station_id(recipe->station);
    if (station < 0)
        station = ROGUE_CRAFT_STATION_WORKBENCH;
    /* enforce waiting queue length */
    int waiting_ct = 0;
    for (int i = 0; i < g_job_count; i++)
    {
        if (g_jobs[i].station == station && g_jobs[i].state == 0)
            waiting_ct++;
    }
    if (waiting_ct >= station_wait_cap[station])
        return -7; /* waiting queue full */
    int speed_pct = rogue_craft_perk_speed_pct(disc);
    if (speed_pct < 1)
        speed_pct = 100;
    if (speed_pct > 100)
        speed_pct = 100;
    RogueCraftJob* j = &g_jobs[g_job_count++];
    j->id = g_next_id++;
    j->recipe_index = recipe_index;
    j->station = station;
    int base_time = recipe->time_ms > 0 ? recipe->time_ms : 1;
    j->total_ms = (base_time * speed_pct) / 100;
    if (j->total_ms < 1)
        j->total_ms = 1;
    j->remaining_ms = j->total_ms;
    j->state = 0; /* waiting -> activation pass */
    try_activate_waiting();
    return j->id;
}

/**
 * @brief Advance crafting jobs by the elapsed time and collect outputs.
 *
 * This function should be called with the frame/tick delta in milliseconds.
 * Active jobs (state == 1) have their remaining time reduced. When a job
 * reaches zero remaining time it is moved to the ready-to-deliver state
 * (state == 2). Ready jobs are then processed exactly once: their output
 * item(s) are added via @c inv_add, duplicate output chance is applied using
 * perks, experience is awarded, and discovery unlocks are triggered.
 *
 * @param delta_ms Milliseconds elapsed since the last update; must be > 0.
 * @param inv_add Callback used to add items to the inventory when a job
 *                completes.
 */
void rogue_craft_queue_update(int delta_ms, RogueInvAddFn inv_add)
{
    if (delta_ms <= 0 || !inv_add)
        return;
    for (int i = 0; i < g_job_count; i++)
    {
        RogueCraftJob* j = &g_jobs[i];
        if (j->state == 1)
        {
            j->remaining_ms -= delta_ms;
            if (j->remaining_ms <= 0)
            {
                j->state = 2;
                j->remaining_ms = 0;
            }
        }
    }
    /* issue outputs exactly once */
    for (int i = 0; i < g_job_count; i++)
    {
        RogueCraftJob* j = &g_jobs[i];
        if (j->state == 2)
        {
            const RogueCraftRecipe* r = rogue_craft_recipe_at(j->recipe_index);
            if (r)
            {
                enum RogueCraftDiscipline disc = rogue_craft_station_discipline(j->station);
                inv_add(r->output_def, r->output_qty);
                /* duplicate chance */
                int dup_chance = rogue_craft_perk_duplicate_chance_pct(disc);
                if (dup_chance > 0)
                {
                    unsigned hash = (unsigned) (j->id * 2654435761u);
                    if ((int) (hash % 100) < dup_chance)
                    {
                        inv_add(r->output_def, r->output_qty);
                    }
                }
                if (r->exp_reward > 0)
                    rogue_craft_skill_gain(disc, r->exp_reward);
                /* Unlock any recipes that depend on this output as an input (discovery system) */
                rogue_craft_discovery_unlock_dependencies(j->recipe_index);
            }
            j->state = 4; /* delivered */
        }
    }
    try_activate_waiting();
}

/**
 * @brief Cancel a queued or active job and refund (part of) its materials.
 *
 * If the job is waiting (state == 0) a full refund of the input quantities
 * is given. If the job is active but not yet completed a half-refund (rounded
 * down) is returned. Jobs that are completed/delivered, or already canceled,
 * cannot be canceled.
 *
 * Error codes:
 *  - -1 : invalid arguments
 *  - -2 : job already completed/delivered or already canceled
 *  - -3 : job id not found
 *
 * @param job_id The id returned by `rogue_craft_queue_enqueue`.
 * @param recipe Pointer to the recipe used by the job (used for refunds).
 * @param inv_add Callback to add refunded items back into inventory.
 * @return 0 on success, or negative error code.
 */
int rogue_craft_queue_cancel(int job_id, const RogueCraftRecipe* recipe, RogueInvAddFn inv_add)
{
    if (job_id <= 0 || !recipe || !inv_add)
        return -1;
    for (int i = 0; i < g_job_count; i++)
    {
        RogueCraftJob* j = &g_jobs[i];
        if (j->id == job_id)
        {
            if (j->state == 2 || j->state == 4 || j->state == 3)
                return -2; /* completed/delivered or already canceled */
            int full = (j->state == 0);
            j->state = 3; /* canceled */
            for (int k = 0; k < recipe->input_count; k++)
            {
                int qty = recipe->inputs[k].quantity;
                if (!full)
                {
                    qty = qty / 2;
                }
                if (qty > 0)
                    inv_add(recipe->inputs[k].def_index, qty);
            }
            try_activate_waiting();
            return 0;
        }
    }
    return -3;
}
