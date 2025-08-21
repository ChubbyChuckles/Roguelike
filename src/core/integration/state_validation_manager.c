#include "state_validation_manager.h"
#include "snapshot_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef ROGUE_VALID_MAX_SYSTEMS
#define ROGUE_VALID_MAX_SYSTEMS 64
#endif
#ifndef ROGUE_VALID_MAX_CROSS
#define ROGUE_VALID_MAX_CROSS 64
#endif
#ifndef ROGUE_VALID_EVENT_CAP
#define ROGUE_VALID_EVENT_CAP 256
#endif

typedef struct SystemReg
{
    int used;
    int system_id;
    RogueSystemValidateFn fn;
    RogueSystemRepairFn repair;
    void* user;
    uint64_t last_hash; // snapshot hash baseline for incremental detection
} SystemReg;

typedef struct CrossReg
{
    int used;
    RogueCrossRuleFn fn;
    void* user;
    char name[32];
} CrossReg;

static SystemReg g_systems[ROGUE_VALID_MAX_SYSTEMS];
static CrossReg g_cross[ROGUE_VALID_MAX_CROSS];
static RogueValidationStats g_stats;
static RogueValidationEvent g_events[ROGUE_VALID_EVENT_CAP];
static size_t g_event_count = 0;
static size_t g_event_head = 0;
static uint64_t g_event_seq = 0;
static uint32_t g_interval_ticks = 0;
static uint64_t g_last_run_tick = 0;
static int g_pending = 0;

static void log_event(uint64_t tick, int system_id, RogueValidationResult r, int repaired,
                      int repair_success)
{
    RogueValidationEvent* ev = &g_events[g_event_head];
    ev->seq = ++g_event_seq;
    ev->tick = tick;
    ev->system_id = system_id;
    ev->severity = r.severity;
    ev->code = r.code;
    ev->repair_attempted = (uint8_t) repaired;
    ev->repair_success = (uint8_t) repair_success;
    ev->message[0] = '\0';
    if (r.message)
    {
        size_t n = strlen(r.message);
        if (n > sizeof(ev->message) - 1)
            n = sizeof(ev->message) - 1;
        memcpy(ev->message, r.message, n);
        ev->message[n] = '\0';
    }
    g_event_head = (g_event_head + 1) % ROGUE_VALID_EVENT_CAP;
    if (g_event_count < ROGUE_VALID_EVENT_CAP)
        g_event_count++;
}

int rogue_validation_register_system(int system_id, RogueSystemValidateFn fn,
                                     RogueSystemRepairFn repair, void* user)
{
    if (system_id < 0)
        return -1;
    for (int i = 0; i < ROGUE_VALID_MAX_SYSTEMS; i++)
    {
        if (g_systems[i].used && g_systems[i].system_id == system_id)
            return -2;
    }
    for (int i = 0; i < ROGUE_VALID_MAX_SYSTEMS; i++)
    {
        if (!g_systems[i].used)
        {
            g_systems[i].used = 1;
            g_systems[i].system_id = system_id;
            g_systems[i].fn = fn;
            g_systems[i].repair = repair;
            g_systems[i].user = user;
            g_systems[i].last_hash = 0;
            return 0;
        }
    }
    return -3;
}
int rogue_validation_register_cross_rule(const char* name, RogueCrossRuleFn fn, void* user)
{
    if (!fn)
        return -1;
    for (int i = 0; i < ROGUE_VALID_MAX_CROSS; i++)
    {
        if (!g_cross[i].used)
        {
            g_cross[i].used = 1;
            g_cross[i].fn = fn;
            g_cross[i].user = user;
            if (name)
            {
                size_t n = strlen(name);
                if (n > sizeof(g_cross[i].name) - 1)
                    n = sizeof(g_cross[i].name) - 1;
                memcpy(g_cross[i].name, name, n);
                g_cross[i].name[n] = '\0';
            }
            else
                g_cross[i].name[0] = '\0';
            return 0;
        }
    }
    return -2;
}

void rogue_validation_set_interval(uint32_t ticks) { g_interval_ticks = ticks; }
void rogue_validation_trigger(void) { g_pending = 1; }

static int run_internal(int force_all)
{
    uint64_t tick = g_last_run_tick;
    g_stats.runs_initiated++;
    // systems
    for (int i = 0; i < ROGUE_VALID_MAX_SYSTEMS; i++)
        if (g_systems[i].used)
        {
            SystemReg* sr = &g_systems[i];
            int skip = 0;
            const RogueSystemSnapshot* snap = rogue_snapshot_get(sr->system_id);
            if (snap)
            {
                if (!force_all && sr->last_hash == snap->hash)
                {
                    g_stats.system_validations_skipped_unchanged++;
                    skip = 1;
                }
                else
                {
                    sr->last_hash = snap->hash;
                }
            }
            if (skip)
                continue;
            if (sr->fn)
            {
                RogueValidationResult r = sr->fn(sr->user);
                g_stats.system_validations_run++;
                if (r.severity == ROGUE_VALID_WARN)
                    g_stats.warnings++;
                else if (r.severity == ROGUE_VALID_CORRUPT)
                    g_stats.corruptions_detected++;
                int repaired = 0, success = 0;
                if (r.severity == ROGUE_VALID_CORRUPT && sr->repair)
                {
                    repaired = 1;
                    g_stats.repairs_attempted++;
                    if (sr->repair(sr->user, r.code) == 0)
                    {
                        success = 1;
                        g_stats.repairs_succeeded++;
                    }
                }
                log_event(tick, sr->system_id, r, repaired, success);
            }
        }
    // cross rules
    for (int i = 0; i < ROGUE_VALID_MAX_CROSS; i++)
        if (g_cross[i].used)
        {
            CrossReg* cr = &g_cross[i];
            if (cr->fn)
            {
                RogueValidationResult r = cr->fn(cr->user);
                g_stats.cross_rule_runs++;
                if (r.severity == ROGUE_VALID_WARN)
                    g_stats.warnings++;
                else if (r.severity == ROGUE_VALID_CORRUPT)
                    g_stats.corruptions_detected++;
                log_event(tick, -1, r, 0, 0);
            }
        }
    g_stats.runs_completed++;
    return 0;
}

int rogue_validation_run_now(int force_all) { return run_internal(force_all); }

void rogue_validation_tick(uint64_t current_tick)
{
    if (current_tick == 0)
        current_tick = 1; // simple guard
    if (g_last_run_tick == 0)
        g_last_run_tick = current_tick - 1; // initialize baseline
    if (g_interval_ticks == 0 && !g_pending)
        return; // disabled
    if (!g_pending && current_tick - g_last_run_tick < g_interval_ticks)
        return;
    g_last_run_tick = current_tick;
    g_pending = 0;
    run_internal(0);
}

void rogue_validation_get_stats(RogueValidationStats* out)
{
    if (out)
        *out = g_stats;
}
int rogue_validation_events_get(const RogueValidationEvent** out_events, size_t* count)
{
    if (!out_events || !count)
        return -1;
    if (g_event_count == 0)
    {
        *out_events = NULL;
        *count = 0;
        return 0;
    }
    *out_events = g_events;
    *count = g_event_count;
    return 0;
}
void rogue_validation_dump(void* fptr)
{
    FILE* f = fptr ? (FILE*) fptr : stdout;
    fprintf(f,
            "[validation] runs=%llu done=%llu sys=%llu skipped=%llu cross=%llu warn=%llu "
            "corrupt=%llu repairs=%llu/%llu\n",
            (unsigned long long) g_stats.runs_initiated,
            (unsigned long long) g_stats.runs_completed,
            (unsigned long long) g_stats.system_validations_run,
            (unsigned long long) g_stats.system_validations_skipped_unchanged,
            (unsigned long long) g_stats.cross_rule_runs, (unsigned long long) g_stats.warnings,
            (unsigned long long) g_stats.corruptions_detected,
            (unsigned long long) g_stats.repairs_succeeded,
            (unsigned long long) g_stats.repairs_attempted);
    size_t count = 0;
    const RogueValidationEvent* evs = NULL;
    rogue_validation_events_get(&evs, &count);
    for (size_t i = 0; i < count; i++)
    {
        fprintf(f, " evt%llu tick=%llu sys=%d sev=%d code=%u repaired=%u ok=%u msg=%s\n",
                (unsigned long long) evs[i].seq, (unsigned long long) evs[i].tick, evs[i].system_id,
                evs[i].severity, evs[i].code, evs[i].repair_attempted, evs[i].repair_success,
                evs[i].message);
    }
}
void rogue_validation_reset_all(void)
{
    memset(g_systems, 0, sizeof(g_systems));
    memset(g_cross, 0, sizeof(g_cross));
    memset(&g_stats, 0, sizeof(g_stats));
    memset(g_events, 0, sizeof(g_events));
    g_event_count = g_event_head = g_event_seq = 0;
    g_interval_ticks = 0;
    g_last_run_tick = 0;
    g_pending = 0;
}
