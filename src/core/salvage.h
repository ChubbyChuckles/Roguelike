/* Salvage System (11.1 initial) */
#ifndef ROGUE_SALVAGE_H
#define ROGUE_SALVAGE_H

#include "core/loot_item_defs.h"

/* Returns quantity of material produced, or 0 if not salvageable. Adds material to inventory via provided callback. */
int rogue_salvage_item(int item_def_index, int rarity, int (*add_material_cb)(int def_index,int qty));

#endif
