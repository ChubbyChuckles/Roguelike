#ifndef ROGUE_VERSIONING_H
#define ROGUE_VERSIONING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Generic data structure versioning & migration framework.
// Types are identified by string name; each has a current version (monotonic uint32).
// Migrations are registered as stepwise transforms from version N -> N+1.
// A migration function may reallocate the data buffer (it receives & updates pointer & size).
// On failure during a multi-step migration chain, original data is restored (rollback) and error returned.

typedef int (*RogueMigrationFn)(void **data, size_t *size, void *user);

// Registration
int rogue_version_register_type(const char *type_name, uint32_t current_version);
int rogue_version_register_migration(const char *type_name, uint32_t from_version, uint32_t to_version, RogueMigrationFn fn, void *user_data);

// Query
uint32_t rogue_version_get_current(const char *type_name);

// Migration progress stats for profiling & debugging.
typedef struct RogueMigrationProgress {
    uint32_t steps_total;
    uint32_t steps_completed;
    uint32_t last_from;
    uint32_t last_to;
    uint32_t fail_from;
    uint32_t fail_to;
    int failed; // non-zero if failure
} RogueMigrationProgress;

// Perform migration of an instance from source_version to target_version (or to current if target_version==0).
// data_ptr points to a buffer pointer holding the instance; may be reallocated.
// size_ptr updated if size changes. Progress optional.
// Returns 0 on success, non-zero on error (unknown type, missing path, migration failure).
int rogue_version_migrate(const char *type_name, uint32_t source_version, uint32_t target_version, void **data_ptr, size_t *size_ptr, RogueMigrationProgress *progress_out);

// Diagnostics / stats
typedef struct RogueVersioningStats {
    uint32_t types_registered;
    uint32_t migrations_registered;
    uint64_t migrations_executed;
    uint64_t migration_steps;
    uint64_t migration_failures;
} RogueVersioningStats;

void rogue_versioning_get_stats(RogueVersioningStats *out);
void rogue_versioning_dump(void *fptr);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_VERSIONING_H
