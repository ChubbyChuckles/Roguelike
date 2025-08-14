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
} RogueItemInstance;

void rogue_items_init_runtime(void);
void rogue_items_shutdown_runtime(void);
int rogue_items_spawn(int def_index, int quantity, float x, float y);
int rogue_items_active_count(void);
void rogue_items_update(float dt_ms);

#endif
