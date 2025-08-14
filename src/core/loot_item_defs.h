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
} RogueItemDef;

/* Runtime registry */
int rogue_item_defs_load_from_cfg(const char* path); /* returns count added or <0 on error */
const RogueItemDef* rogue_item_def_by_id(const char* id);
int rogue_item_def_index(const char* id); /* -1 if not found */
int rogue_item_defs_count(void);
void rogue_item_defs_reset(void);

#endif
