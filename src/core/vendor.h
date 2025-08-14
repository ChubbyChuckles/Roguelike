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

typedef struct RogueVendorRotation {
    int loot_table_indices[8]; /* up to 8 candidate tables for rotation */
    int table_count;
    int current_table; /* index into loot_table_indices */
    float restock_interval_ms; /* how often to restock */
    float time_accum_ms; /* internal timer */
} RogueVendorRotation;

void rogue_vendor_reset(void);
/* Generate up to slots items from loot table; returns count generated */
int rogue_vendor_generate_inventory(int loot_table_index, int slots, const RogueGenerationContext* ctx, unsigned int* rng_state);
int rogue_vendor_item_count(void);
const RogueVendorItem* rogue_vendor_get(int index);
int rogue_vendor_price_formula(int item_def_index, int rarity);

/* Rotation / Restock (10.3) */
void rogue_vendor_rotation_init(RogueVendorRotation* rot, float interval_ms);
int rogue_vendor_rotation_add_table(RogueVendorRotation* rot, int loot_table_index);
/* Advances timer; if interval elapsed performs restock (regenerates inventory) returning 1, else 0. */
int rogue_vendor_update_and_maybe_restock(RogueVendorRotation* rot, float dt_ms, const RogueGenerationContext* ctx, unsigned int* seed, int slots);
int rogue_vendor_current_table(const RogueVendorRotation* rot);

#endif /* ROGUE_VENDOR_H */
