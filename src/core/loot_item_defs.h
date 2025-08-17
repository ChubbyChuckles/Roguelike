#ifndef ROGUE_LOOT_ITEM_DEFS_H
#define ROGUE_LOOT_ITEM_DEFS_H
#include <stdint.h>

/* Phase 1: Base item definitions (no affixes yet) */
#define ROGUE_MAX_ITEM_ID_LEN 32
#define ROGUE_MAX_ITEM_NAME_LEN 48
#ifndef ROGUE_ITEM_DEF_CAP
#define ROGUE_ITEM_DEF_CAP 512
#endif

typedef enum RogueItemCategory {
    ROGUE_ITEM_MISC=0,
    ROGUE_ITEM_CONSUMABLE,
    ROGUE_ITEM_WEAPON,
    ROGUE_ITEM_ARMOR,
    ROGUE_ITEM_GEM,
    ROGUE_ITEM_MATERIAL,
    ROGUE_ITEM__COUNT
} RogueItemCategory;

typedef struct RogueItemDef {
    char id[ROGUE_MAX_ITEM_ID_LEN];
    char name[ROGUE_MAX_ITEM_NAME_LEN];
    RogueItemCategory category;
    int level_req;
    int stack_max; /* 1 = not stackable */
    int base_value; /* gold */
    int base_damage_min;
    int base_damage_max;
    int base_armor;
    char sprite_sheet[128];
    int sprite_tx, sprite_ty, sprite_tw, sprite_th;
    int rarity; /* RogueItemRarity enum value */
} RogueItemDef;

/* Runtime registry */
int rogue_item_defs_load_from_cfg(const char* path); /* returns count added or <0 on error */
/* Convenience: load multiple item definition cfg files from a directory of category files (swords.cfg, potions.cfg, etc.).
 * Implementation keeps simple portability: attempts to open a known set of filenames inside dir.
 * Returns total successfully added (aggregate) or <0 if dir invalid. */
int rogue_item_defs_load_directory(const char* dir_path);
const RogueItemDef* rogue_item_def_by_id(const char* id);
int rogue_item_def_index(const char* id); /* -1 if not found */
int rogue_item_defs_count(void);
void rogue_item_defs_reset(void);
/* Internal convenience: get pointer by index (returns NULL if OOB) */
const RogueItemDef* rogue_item_def_at(int index);
/* Phase 17.4: Cache-friendly fast index lookups via open-addressed hash built post-load */
int rogue_item_defs_build_index(void); /* rebuilds hash index (call after bulk load); returns 0 on success */
int rogue_item_def_index_fast(const char* id); /* O(1) average hash lookup; falls back to linear if hash not built */

#endif
