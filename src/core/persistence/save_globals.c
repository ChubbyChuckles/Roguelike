#include "../../util/log.h"
#include "save_internal.h"

/* Global state definitions */
RogueSaveComponent g_save_components[ROGUE_SAVE_MAX_COMPONENTS];
int g_save_component_count = 0;
int g_save_initialized = 0;
int g_save_migrations_registered = 0;
uint32_t g_save_dirty_mask = 0xFFFFFFFFu;
int g_save_incremental_enabled = 0;
int g_save_debug_json_dump = 0;
int g_save_durable_writes = 0;
unsigned char g_save_last_sha256[32];
uint32_t g_save_last_tamper_flags = 0;
int g_save_last_recovery_used = 0;
int g_save_autosave_interval_ms = 0;
int g_save_autosave_throttle_ms = 0;
uint32_t g_save_autosave_count = 0;
int g_save_last_rc = 0;
uint32_t g_save_last_bytes = 0;
double g_save_last_ms = 0.0;

uint32_t g_active_write_version = 0;
uint32_t g_active_read_version = 0;

const struct RogueSaveSignatureProvider* g_save_sig_provider = NULL;

/* Migrations */
RogueSaveMigration g_save_migrations[16];
int g_save_migration_count = 0;
int g_save_last_migration_steps = 0;
int g_save_last_migration_failed = 0;
double g_save_last_migration_ms = 0.0;

RogueCachedSection g_save_cached_sections[ROGUE_SAVE_MAX_COMPONENTS];
unsigned g_save_last_sections_reused = 0;
unsigned g_save_last_sections_written = 0;

const RogueSaveComponent* rogue_find_component(int id)
{
    for (int i = 0; i < g_save_component_count; i++)
        if (g_save_components[i].id == id)
            return &g_save_components[i];
    return NULL;
}
