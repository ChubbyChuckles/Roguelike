#ifndef ROGUE_SAVE_MANAGER_H
#define ROGUE_SAVE_MANAGER_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Phase 1 Core Save Architecture scaffold */

#define ROGUE_SAVE_MAX_COMPONENTS 16
#define ROGUE_SAVE_SLOT_COUNT 3
#define ROGUE_AUTOSAVE_RING 4

/* Current binary save format version */
#define ROGUE_SAVE_FORMAT_VERSION 7u /* v3: TLV headers (uint16 id + uint32 size); v4: varint counts/ids in section payloads; v5: string interning; v6: optional per-section compression; v7: per-section CRC32 + overall SHA256 footer */

/* Component identifiers (stable) */
typedef enum RogueSaveComponentId {
    ROGUE_SAVE_COMP_PLAYER=1,
    ROGUE_SAVE_COMP_WORLD_META=2,
    ROGUE_SAVE_COMP_INVENTORY=3,
    ROGUE_SAVE_COMP_SKILLS=4,
    ROGUE_SAVE_COMP_BUFFS=5,
    ROGUE_SAVE_COMP_VENDOR=6,
    ROGUE_SAVE_COMP_STRINGS=7, /* v5 string intern table (optional) */
} RogueSaveComponentId;

/* Descriptor header written at start of save file */
typedef struct RogueSaveDescriptor {
    uint32_t version;      /* format version */
    uint32_t timestamp_unix; /* seconds */
    uint32_t component_mask; /* bit per component id (1<<id) */
    uint32_t section_count;  /* number of serialized sections */
    uint64_t total_size;     /* total bytes of file (for sanity) */
    uint32_t checksum;       /* simple CRC32 of following bytes (Phase4 will extend) */
} RogueSaveDescriptor;

/* Version semantics:
    v1: Initial binary prototype (unused in current tree, legacy test fabricates header only)
    v2: Introduced migration runner + metrics (fixed 8-byte section headers: uint32 id + uint32 size)
    v3: Switched to compact TLV headers: uint16 id + uint32 size per section (format change)
    v4: Introduced varint (LEB128) encoding for section-local counts & ids (inventory item count, item id fields, skill count & ranks, buff count)
    v5: Added optional string interning table section (component id 7) containing unique strings referenced indirectly by future sections
    v6: Optional per-section RLE compression (flag in size high bit for version>=6) with uncompressed size prefix (uint32)
    v7: Integrity hardening: per-section CRC32 (of uncompressed payload) appended after each section payload + 4-byte CRC, and overall SHA256 footer ("SH32" magic + 32 raw bytes) */


/* Component callback interface */
typedef struct RogueSaveComponent {
    int id; /* RogueSaveComponentId */
    int (*write_fn)(FILE* f); /* returns 0 on success */
    int (*read_fn)(FILE* f, size_t size); /* size is section payload */
    const char* name;
} RogueSaveComponent;

/* Migration registry (Phase 2) */
typedef struct RogueSaveMigration {
    uint32_t from_version;
    uint32_t to_version; /* should be from_version+1 for linear chain */
    int (*apply_fn)(unsigned char* data, size_t size); /* mutate in-place, return 0 on success */
    const char* name;    
} RogueSaveMigration;

void rogue_save_manager_init(void);
void rogue_save_manager_register(const RogueSaveComponent* comp);
int rogue_save_manager_save_slot(int slot_index); /* full save */
int rogue_save_manager_load_slot(int slot_index);
int rogue_save_manager_autosave(int slot_index); /* autosave ring save */
void rogue_register_core_save_components(void); /* registers Phase 1 core components */

/* Migration API */
void rogue_save_register_migration(const RogueSaveMigration* mig);
int rogue_save_manager_set_durable(int enabled); /* enable fsync/_commit durability */

/* Migration metrics (Phase 2.4) */
int rogue_save_last_migration_steps(void); /* number of successful version bumps in last load */
int rogue_save_last_migration_failed(void); /* 1 if a migration apply_fn failed */
double rogue_save_last_migration_ms(void); /* total time spent applying migrations (ms) */

/* Debug & Testing (Phase 3 / 10) */
int rogue_save_reload_component_from_slot(int slot_index, int component_id); /* reapply a single component payload from saved slot (no checksum rewrite) */
int rogue_save_export_json(int slot_index, char* out, size_t out_cap); /* produce lightweight JSON summary (header + section list) */
int rogue_save_set_debug_json(int enabled); /* when enabled, writes full JSON dump alongside each save (save_slot_X.json) */
typedef int (*RogueSaveSectionIterFn)(const struct RogueSaveDescriptor* desc, uint32_t id, const void* data, uint32_t size, void* user);
int rogue_save_for_each_section(int slot_index, RogueSaveSectionIterFn fn, void* user); /* iterate validated sections (post checksum) */

/* String interning API (Phase 3.5) */
int rogue_save_intern_string(const char* s); /* returns index (>=0) or -1 on failure */
const char* rogue_save_intern_get(int index); /* returns interned string or NULL */
int rogue_save_intern_count(void);

/* Compression (Phase 3.6) */
int rogue_save_set_compression(int enabled, int min_bytes); /* enable and set minimum payload size to consider compression */

/* Endianness / numeric width assertion helper (Phase 3.3) */
int rogue_save_format_endianness_is_le(void); /* returns 1 if little-endian (required). */

/* Test helpers */
void rogue_save_manager_reset_for_tests(void);

/* Testing helpers */
uint32_t rogue_crc32(const void* data, size_t len);
const unsigned char* rogue_save_last_sha256(void); /* returns pointer to 32-byte array (v7+) */
void rogue_save_last_sha256_hex(char out[65]); /* null-terminated hex of last SHA256 (v7+) */

/* Error codes (negative) beyond basic IO */
#define ROGUE_SAVE_ERR_MIGRATION_FAIL   -20
#define ROGUE_SAVE_ERR_MIGRATION_CHAIN  -21
#define ROGUE_SAVE_ERR_SECTION_CRC      -22
#define ROGUE_SAVE_ERR_SHA256           -23

#endif
