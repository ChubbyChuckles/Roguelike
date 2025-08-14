#ifndef ROGUE_INVENTORY_H
#define ROGUE_INVENTORY_H

#include "core/loot_item_defs.h"

void rogue_inventory_init(void);
void rogue_inventory_reset(void);
int  rogue_inventory_add(int def_index, int quantity);
int  rogue_inventory_get_count(int def_index);
int  rogue_inventory_total_distinct(void);

#endif /* ROGUE_INVENTORY_H */
