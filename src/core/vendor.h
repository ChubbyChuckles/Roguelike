/* Vendor economy scaffolding (10.1, 10.2)
 * Minimal vendor inventory generation using existing loot pipeline.
 * Prices derived from base item value scaled by rarity multiplier.
 */
#ifndef ROGUE_VENDOR_H
#define ROGUE_VENDOR_H

#include "core/loot_generation.h"

#ifndef ROGUE_VENDOR_SLOT_CAP
#define ROGUE_VENDOR_SLOT_CAP 32
#endif

typedef struct RogueVendorItem {
    int def_index;
    int rarity;
    int price; /* computed gold cost */
} RogueVendorItem;

void rogue_vendor_reset(void);
/* Generate up to slots items from loot table; returns count generated */
int rogue_vendor_generate_inventory(int loot_table_index, int slots, const RogueGenerationContext* ctx, unsigned int* rng_state);
int rogue_vendor_item_count(void);
const RogueVendorItem* rogue_vendor_get(int index);
int rogue_vendor_price_formula(int item_def_index, int rarity);

#endif /* ROGUE_VENDOR_H */
