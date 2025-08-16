#ifndef ROGUE_SAVE_MANAGER_H
#define ROGUE_SAVE_MANAGER_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

/* Phase 1 Core Save Architecture scaffold */

#define ROGUE_SAVE_MAX_COMPONENTS 16
#define ROGUE_SAVE_SLOT_COUNT 3
#define ROGUE_AUTOSAVE_RING 4

/* Component identifiers (stable) */
typedef enum RogueSaveComponentId {
    ROGUE_SAVE_COMP_PLAYER=1,
    ROGUE_SAVE_COMP_WORLD_META=2,
    ROGUE_SAVE_COMP_INVENTORY=3,
    ROGUE_SAVE_COMP_SKILLS=4,
    ROGUE_SAVE_COMP_BUFFS=5,
    ROGUE_SAVE_COMP_VENDOR=6,
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

/* Component callback interface */
typedef struct RogueSaveComponent {
    int id; /* RogueSaveComponentId */
    int (*write_fn)(FILE* f); /* returns 0 on success */
    int (*read_fn)(FILE* f, size_t size); /* size is section payload */
    const char* name;
} RogueSaveComponent;

void rogue_save_manager_init(void);
void rogue_save_manager_register(const RogueSaveComponent* comp);
int rogue_save_manager_save_slot(int slot_index); /* full save */
int rogue_save_manager_load_slot(int slot_index);

/* Testing helpers */
uint32_t rogue_crc32(const void* data, size_t len);

#endif
