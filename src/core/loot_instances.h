#ifndef ROGUE_LOOT_INSTANCES_H
#define ROGUE_LOOT_INSTANCES_H
#include <stdint.h>
#include "core/loot_item_defs.h"

#ifndef ROGUE_ITEM_INSTANCE_CAP
#define ROGUE_ITEM_INSTANCE_CAP 256
#endif

#ifndef ROGUE_ITEM_DESPAWN_MS
#define ROGUE_ITEM_DESPAWN_MS 60000 /* 60s default */
#endif

#ifndef ROGUE_ITEM_STACK_MERGE_RADIUS
#define ROGUE_ITEM_STACK_MERGE_RADIUS 0.45f
#endif

typedef struct RogueItemInstance {
    int def_index; /* base item definition index */
    int quantity;  /* stack size */
    float x,y;     /* world tile position */
    float life_ms; /* for future despawn */
    int active;
    int rarity; /* cached from definition for render color */
    /* Affix attachments (7.5): one prefix + one suffix for initial implementation */
    int prefix_index; /* -1 if none */
    int prefix_value; /* rolled stat value */
    int suffix_index; /* -1 if none */
    int suffix_value; /* rolled stat value */
} RogueItemInstance;

void rogue_items_init_runtime(void);
void rogue_items_shutdown_runtime(void);
int rogue_items_spawn(int def_index, int quantity, float x, float y);
int rogue_items_active_count(void);
void rogue_items_update(float dt_ms);

/* Affix APIs (7.5 / 7.6) */
int rogue_item_instance_generate_affixes(int inst_index, unsigned int* rng_state, int rarity);
const RogueItemInstance* rogue_item_instance_at(int index);
/* Derived damage range including affix flat damage bonuses */
int rogue_item_instance_damage_min(int inst_index);
int rogue_item_instance_damage_max(int inst_index);
/* Apply affix + rarity data to existing active instance (used for persistence load). */
int rogue_item_instance_apply_affixes(int inst_index, int rarity, int prefix_index, int prefix_value, int suffix_index, int suffix_value);

#endif
