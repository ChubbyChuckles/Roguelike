#ifndef ROGUE_LOOT_INSTANCES_H
#define ROGUE_LOOT_INSTANCES_H
#include <stdint.h>
#include "core/loot_item_defs.h"

#ifndef ROGUE_ITEM_INSTANCE_CAP
#define ROGUE_ITEM_INSTANCE_CAP 256
#endif

typedef struct RogueItemInstance {
    int def_index; /* base item definition index */
    int quantity;  /* stack size */
    float x,y;     /* world tile position */
    float life_ms; /* for future despawn */
    int active;
} RogueItemInstance;

void rogue_items_init_runtime(void);
void rogue_items_shutdown_runtime(void);
int rogue_items_spawn(int def_index, int quantity, float x, float y);
int rogue_items_active_count(void);
void rogue_items_update(float dt_ms);

#endif
