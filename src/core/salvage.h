/* Salvage System (11.1 initial) */
#ifndef ROGUE_SALVAGE_H
#define ROGUE_SALVAGE_H

#include "core/loot_item_defs.h"

/* Returns quantity of material produced, or 0 if not salvageable. Adds material to inventory via provided callback. */
int rogue_salvage_item(int item_def_index, int rarity, int (*add_material_cb)(int def_index,int qty));

/* Phase 8.4 extended API: allow instance-aware salvage to factor current durability (if applicable).
	If inst_index>=0 and references a valid active instance with durability, yield is scaled by
		salvage_durability_factor = 0.4 + 0.6 * (cur_durability / max_durability)
	so broken items still give 40% baseline.
	Returns produced quantity. */
int rogue_salvage_item_instance(int inst_index, int (*add_material_cb)(int def_index,int qty));

#endif
