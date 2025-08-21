#include "versioning.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VERSIONING_MAX_TYPES
#define VERSIONING_MAX_TYPES 256
#endif
#ifndef VERSIONING_MAX_MIGRATIONS
#define VERSIONING_MAX_MIGRATIONS 2048
#endif

typedef struct RogueMigration
{
    uint32_t from_v;
    uint32_t to_v;
    RogueMigrationFn fn;
    void* user;
} RogueMigration;

typedef struct RogueTypeInfo
{
    char* name;
    uint32_t current_version;
    // Dynamic array of migrations (from->to contiguous steps)
    RogueMigration* migs;
    uint32_t mig_count;
    uint32_t mig_cap;
} RogueTypeInfo;

static RogueTypeInfo g_types[VERSIONING_MAX_TYPES];
static uint32_t g_type_count;

static struct
{
    uint32_t types_registered;
    uint32_t migrations_registered;
    uint64_t migrations_executed;
    uint64_t migration_steps;
    uint64_t migration_failures;
} g_stats;

static RogueTypeInfo* find_type(const char* name)
{
    for (uint32_t i = 0; i < g_type_count; i++)
        if (strcmp(g_types[i].name, name) == 0)
            return &g_types[i];
    return NULL;
}

int rogue_version_register_type(const char* type_name, uint32_t current_version)
{
    if (!type_name || current_version == 0)
        return -1;
    if (find_type(type_name))
        return -1; // already
    if (g_type_count >= VERSIONING_MAX_TYPES)
        return -1;
    RogueTypeInfo* ti = &g_types[g_type_count++];
    size_t len = strlen(type_name);
    ti->name = (char*) malloc(len + 1);
    if (!ti->name)
    {
        g_type_count--;
        return -1;
    }
    memcpy(ti->name, type_name, len + 1);
    ti->current_version = current_version;
    ti->migs = NULL;
    ti->mig_count = ti->mig_cap = 0;
    g_stats.types_registered++;
    return 0;
}

int rogue_version_register_migration(const char* type_name, uint32_t from_version,
                                     uint32_t to_version, RogueMigrationFn fn, void* user_data)
{
    if (!type_name || !fn || to_version != from_version + 1)
        return -1;
    RogueTypeInfo* ti = find_type(type_name);
    if (!ti)
        return -1;
    // Ensure uniqueness of step
    for (uint32_t i = 0; i < ti->mig_count; i++)
        if (ti->migs[i].from_v == from_version)
            return -1;
    if (ti->mig_count == ti->mig_cap)
    {
        uint32_t new_cap = ti->mig_cap ? ti->mig_cap * 2 : 4;
        RogueMigration* n = (RogueMigration*) realloc(ti->migs, new_cap * sizeof(RogueMigration));
        if (!n)
            return -1;
        ti->migs = n;
        ti->mig_cap = new_cap;
    }
    RogueMigration* m = &ti->migs[ti->mig_count++];
    m->from_v = from_version;
    m->to_v = to_version;
    m->fn = fn;
    m->user = user_data;
    g_stats.migrations_registered++;
    return 0;
}

uint32_t rogue_version_get_current(const char* type_name)
{
    RogueTypeInfo* ti = find_type(type_name);
    return ti ? ti->current_version : 0;
}

static RogueMigration* find_step(RogueTypeInfo* ti, uint32_t from_v)
{
    for (uint32_t i = 0; i < ti->mig_count; i++)
        if (ti->migs[i].from_v == from_v)
            return &ti->migs[i];
    return NULL;
}

int rogue_version_migrate(const char* type_name, uint32_t source_version, uint32_t target_version,
                          void** data_ptr, size_t* size_ptr, RogueMigrationProgress* progress_out)
{
    if (!type_name || !data_ptr || !size_ptr)
        return -1;
    RogueTypeInfo* ti = find_type(type_name);
    if (!ti)
        return -1;
    if (target_version == 0)
        target_version = ti->current_version;
    if (source_version == target_version)
        return 0; // nothing
    if (source_version > target_version)
        return -1; // downgrade unsupported
    // Snapshot original for rollback
    void* orig_data = *data_ptr;
    size_t orig_size = *size_ptr;

    RogueMigrationProgress prog = {0};
    prog.steps_total = target_version - source_version;
    uint32_t v = source_version;
    while (v < target_version)
    {
        RogueMigration* step = find_step(ti, v);
        if (!step || step->to_v != v + 1)
        {
            prog.failed = 1;
            prog.fail_from = v;
            prog.fail_to = v + 1;
            if (progress_out)
                *progress_out = prog;
            *data_ptr = orig_data;
            *size_ptr = orig_size;
            return -1;
        }
        prog.last_from = v;
        prog.last_to = v + 1;
        g_stats.migration_steps++;
        int rc = step->fn(data_ptr, size_ptr, step->user);
        if (rc != 0)
        {
            prog.failed = 1;
            prog.fail_from = v;
            prog.fail_to = v + 1;
            g_stats.migration_failures++; // rollback
            *data_ptr = orig_data;
            *size_ptr = orig_size;
            if (progress_out)
                *progress_out = prog;
            return -1;
        }
        v = v + 1;
        prog.steps_completed++;
    }
    g_stats.migrations_executed++;
    if (progress_out)
        *progress_out = prog;
    return 0;
}

void rogue_versioning_get_stats(RogueVersioningStats* out)
{
    if (!out)
        return;
    out->types_registered = g_stats.types_registered;
    out->migrations_registered = g_stats.migrations_registered;
    out->migrations_executed = g_stats.migrations_executed;
    out->migration_steps = g_stats.migration_steps;
    out->migration_failures = g_stats.migration_failures;
}

void rogue_versioning_dump(void* fptr)
{
    FILE* f = fptr ? (FILE*) fptr : stdout;
    fprintf(f, "[versioning] types=%u migrations=%u executed=%llu steps=%llu failures=%llu\n",
            g_stats.types_registered, g_stats.migrations_registered,
            (unsigned long long) g_stats.migrations_executed,
            (unsigned long long) g_stats.migration_steps,
            (unsigned long long) g_stats.migration_failures);
    for (uint32_t i = 0; i < g_type_count; i++)
    {
        RogueTypeInfo* ti = &g_types[i];
        fprintf(f, " type '%s' current=%u migs=%u\n", ti->name, ti->current_version, ti->mig_count);
        for (uint32_t j = 0; j < ti->mig_count; j++)
        {
            fprintf(f, "  %u->%u\n", ti->migs[j].from_v, ti->migs[j].to_v);
        }
    }
}
