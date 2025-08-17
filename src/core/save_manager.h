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
#define ROGUE_SAVE_FORMAT_VERSION 9u /* v3: TLV headers (uint16 id + uint32 size); v4: varint counts/ids in section payloads; v5: string interning; v6: optional per-section compression; v7: per-section CRC32 + overall SHA256 footer; v8: replay hash + tamper recovery; v9: optional signature trailer */

/* Component identifiers (stable) */
typedef enum RogueSaveComponentId {
    ROGUE_SAVE_COMP_PLAYER=1,
    ROGUE_SAVE_COMP_WORLD_META=2,
    ROGUE_SAVE_COMP_INVENTORY=3,
    ROGUE_SAVE_COMP_SKILLS=4,
    ROGUE_SAVE_COMP_BUFFS=5,
    ROGUE_SAVE_COMP_VENDOR=6,
    ROGUE_SAVE_COMP_STRINGS=7, /* v5 string intern table (optional) */
    ROGUE_SAVE_COMP_REPLAY=8, /* v8 replay hash section */
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
    v7: Integrity hardening: per-section CRC32 (of uncompressed payload) appended after each section payload + 4-byte CRC, and overall SHA256 footer ("SH32" magic + 32 raw bytes)
    v8: Replay hash section (component id 8) capturing a SHA256 digest of recorded gameplay-critical input events (frame, action, value) + event count for deterministic divergence checks (UI state excluded)
    v9: Optional signature trailer (if registered) appended after SHA256 footer: [uint16 sig_len]["SGN0"][sig_len raw bytes]; descriptor checksum excludes SHA+signature footers */


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
int rogue_save_manager_quicksave(void); /* writes quicksave.sav */
void rogue_register_core_save_components(void); /* registers Phase 1 core components */

/* Migration API */
void rogue_save_register_migration(const RogueSaveMigration* mig);
int rogue_save_manager_set_durable(int enabled); /* enable fsync/_commit durability */
/* Autosave scheduling (Phase 6.1/6.3/6.4) */
int rogue_save_set_autosave_interval_ms(int ms); /* set interval (<=0 disables) */
int rogue_save_manager_update(uint32_t now_ms, int in_combat); /* call periodically; triggers autosave when idle */
int rogue_save_last_save_rc(void); /* result code of last save/autosave/quicksave */
uint32_t rogue_save_last_save_bytes(void); /* size in bytes of last successful save (0 if none) */
double rogue_save_last_save_ms(void); /* duration ms of last save */
uint32_t rogue_save_autosave_count(void); /* number of autosaves performed via scheduler */
int rogue_save_set_autosave_throttle_ms(int ms); /* minimum gap after any save before next autosave */
int rogue_save_status_string(char* buf, size_t cap); /* formatted status summary (UI indicator) */

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

/* Replay hash API (v8) */
void rogue_save_replay_reset(void);
int rogue_save_replay_record_input(uint32_t frame, uint32_t action_code, int32_t value); /* returns 0 on success */
const unsigned char* rogue_save_last_replay_hash(void); /* 32-byte hash (all zeroes if none) */
void rogue_save_last_replay_hash_hex(char out[65]);
uint32_t rogue_save_last_replay_event_count(void);

/* Error codes (negative) beyond basic IO */
#define ROGUE_SAVE_ERR_MIGRATION_FAIL   -20
#define ROGUE_SAVE_ERR_MIGRATION_CHAIN  -21
#define ROGUE_SAVE_ERR_SECTION_CRC      -22
#define ROGUE_SAVE_ERR_SHA256           -23

/* Tamper flags (Phase 4.3) */
#define ROGUE_TAMPER_FLAG_DESCRIPTOR_CRC 0x1u
#define ROGUE_TAMPER_FLAG_SECTION_CRC    0x2u
#define ROGUE_TAMPER_FLAG_SHA256         0x4u
#define ROGUE_TAMPER_FLAG_SIGNATURE      0x8u
uint32_t rogue_save_last_tamper_flags(void); /* bitmask of ROGUE_TAMPER_FLAG_* from last load attempt */

/* Recovery load (Phase 4.4) */
int rogue_save_manager_load_slot_with_recovery(int slot_index); /* returns 0 if primary ok, 1 if recovered via autosave, negative on failure */
int rogue_save_last_recovery_used(void); /* 1 if last recovery load fell back to autosave */

/* Incremental save (Phase 5.1/5.2): dirty tracking + cached section reuse */
int rogue_save_set_incremental(int enabled); /* enable incremental mode */
int rogue_save_mark_component_dirty(int component_id); /* mark a component dirty */
int rogue_save_mark_all_dirty(void); /* mark all registered components dirty */

/* Optional signature abstraction (v9). Register a provider to append & verify a detached signature over the hashed region (identical bytes covered by descriptor CRC & SHA256). */
typedef struct RogueSaveSignatureProvider {
    uint32_t max_len; /* maximum signature length supported */
    int (*sign)(const unsigned char* data, size_t len, unsigned char* out, uint32_t* out_len); /* produce signature */
    int (*verify)(const unsigned char* data, size_t len, const unsigned char* sig, uint32_t sig_len); /* returns 0 on valid */
    const char* name;
} RogueSaveSignatureProvider;
int rogue_save_set_signature_provider(const RogueSaveSignatureProvider* prov); /* returns 0 */
const RogueSaveSignatureProvider* rogue_save_get_signature_provider(void);

#endif
