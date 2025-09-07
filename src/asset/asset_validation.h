/* asset_validation.h - Phase 4: Validation & Integrity System
   Provides:
     - File existence & basic integrity helpers
     - CRC32 checksum computation & registry
     - Missing asset fallback (texture path) registration
     - Simple dependency tracking graph (owner -> dependency list)
     - Usage statistics aggregation over current asset manager state
   Notes:
     This phase intentionally keeps JSON schema validation lightweight by
     deferring to existing content schema modules (sprites, audio, levels, etc.).
     A future enhancement can dispatch based on classification & call those
     schema validators directly. For now we expose a stub hook so tests and
     tooling have an entry point.
*/
#ifndef ROGUE_ASSET_VALIDATION_H
#define ROGUE_ASSET_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ---------- File Integrity ---------- */

/* Returns true if the path exists (regular file) */
bool rogue_asset_file_exists(const char* path);

/* Compute CRC32 of file; returns 0 on failure (ok==false). */
uint32_t rogue_asset_crc32_file(const char* path, bool* ok);

/* Register an expected checksum; duplicates overwrite previous value. */
bool rogue_asset_checksum_register(const char* path, uint32_t crc32);

/* Verify all registered checksums; returns true if all match current files. */
bool rogue_asset_checksum_verify_all(void);

/* Verify a single path immediately (helper). */
bool rogue_asset_checksum_verify_one(const char* path, uint32_t expected_crc32);

/* ---------- Fallback Handling ---------- */

/* Set a global fallback texture path (used when initial acquire path missing). */
void rogue_asset_set_fallback_texture(const char* path);
const char* rogue_asset_get_fallback_texture(void);

/* ---------- Dependency Tracking ---------- */

/* Add dependency edge owner -> dependency (strings are copied). */
bool rogue_asset_dep_add(const char* owner_id, const char* dependency_id);

/* Retrieve dependencies for owner; returns count (<= max). */
size_t rogue_asset_dep_get(const char* owner_id, const char** out, size_t max);

/* ---------- Usage Reporting ---------- */
typedef struct RogueAssetUsageStats
{
    uint32_t texture_records;      /* total texture slots occupied */
    uint32_t audio_records;        /* total audio slots occupied */
    uint32_t textures_failed;      /* texture entries marked load_failed */
    uint32_t audio_failed;         /* audio entries marked load_failed */
    uint32_t textures_with_handle; /* have SDL_Texture pointer */
    uint32_t audio_with_handle;    /* have Mix_Chunk pointer */
} RogueAssetUsageStats;

RogueAssetUsageStats rogue_asset_usage_stats(void);

/* ---------- JSON Validation Hook (stub) ---------- */
/* Returns true if file exists and (for now) has .json extension & non-zero size. */
bool rogue_asset_json_validate_basic(const char* path);

/* Cleanup dynamic allocations (dependencies & checksum registry) */
void rogue_asset_validation_shutdown(void);

#endif /* ROGUE_ASSET_VALIDATION_H */
