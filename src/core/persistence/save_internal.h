#ifndef ROGUE_SAVE_INTERNAL_H
#define ROGUE_SAVE_INTERNAL_H

#include "save_manager.h"
#include "save_utils.h"
#include <stddef.h>
#include <stdint.h>

/* Global save-manager state shared across modules (kept internal, no external linkage). */
extern int g_save_initialized;
extern int g_save_migrations_registered;
extern uint32_t g_save_dirty_mask;
extern int g_save_incremental_enabled;
extern int g_save_debug_json_dump;
extern int g_save_durable_writes;
extern unsigned char g_save_last_sha256[32];
extern uint32_t g_save_last_tamper_flags;
extern int g_save_last_recovery_used;
extern int g_save_autosave_interval_ms;
extern int g_save_autosave_throttle_ms;
extern uint32_t g_save_autosave_count;
extern int g_save_last_rc;
extern uint32_t g_save_last_bytes;
extern double g_save_last_ms;

extern uint32_t g_active_write_version;
extern uint32_t g_active_read_version;

/* Signature provider (v9) */
extern const struct RogueSaveSignatureProvider* g_save_sig_provider;

/* Migration registry & metrics */
extern RogueSaveMigration g_save_migrations[16];
extern int g_save_migration_count;
extern int g_save_last_migration_steps;
extern int g_save_last_migration_failed;
extern double g_save_last_migration_ms;

/* Registered components array */
extern RogueSaveComponent g_save_components[ROGUE_SAVE_MAX_COMPONENTS];
extern int g_save_component_count;

/* Section cache (incremental mode) */
typedef struct RogueCachedSection
{
    int id;
    unsigned char* data;
    uint32_t size;
    uint32_t crc32;
    int valid;
} RogueCachedSection;

extern RogueCachedSection g_save_cached_sections[ROGUE_SAVE_MAX_COMPONENTS];
extern unsigned g_save_last_sections_reused;
extern unsigned g_save_last_sections_written;

/* Paths */
const char* rogue_build_slot_path(int slot);
const char* rogue_build_autosave_path(int logical);
const char* rogue_build_backup_path(int slot, uint32_t ts);

/* Internal helpers shared across compilation units */
const RogueSaveComponent* rogue_find_component(int id);

/* Internal accessors from incremental module (for IO paths) */
int rogue__compress_enabled(void);
int rogue__compress_min_bytes(void);

#endif /* ROGUE_SAVE_INTERNAL_H */
