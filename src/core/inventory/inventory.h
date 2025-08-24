#ifndef ROGUE_INVENTORY_H
#define ROGUE_INVENTORY_H

#include "../loot/loot_item_defs.h"
#include <stdio.h>

void rogue_inventory_init(void);
void rogue_inventory_reset(void);
int rogue_inventory_add(int def_index, int quantity);
int rogue_inventory_get_count(int def_index);
/* Consume (remove) quantity; returns actual removed (may be less if insufficient). */
int rogue_inventory_consume(int def_index, int quantity);
int rogue_inventory_total_distinct(void);
/* Serialize all non-zero counts to file pointer (key=value lines) with prefix INV# */
void rogue_inventory_serialize(FILE* f);
/* Load inventory counts from key,value pair (key already split) */
int rogue_inventory_try_parse_kv(const char* key, const char* val);

#endif /* ROGUE_INVENTORY_H */
