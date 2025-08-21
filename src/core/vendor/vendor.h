/* Vendor economy scaffolding (10.1, 10.2)
 * Minimal vendor inventory generation using existing loot pipeline.
 * Prices derived from base item value scaled by rarity multiplier.
 */
#ifndef ROGUE_VENDOR_H
#define ROGUE_VENDOR_H

#include "core/loot/loot_generation.h"

#ifndef ROGUE_VENDOR_SLOT_CAP
#define ROGUE_VENDOR_SLOT_CAP 32
#endif

typedef struct RogueVendorItem
{
    int def_index;
    int rarity;
    int price; /* computed gold cost */
} RogueVendorItem;

typedef struct RogueVendorRotation
{
    int loot_table_indices[8]; /* up to 8 candidate tables for rotation */
    int table_count;
    int current_table;         /* index into loot_table_indices */
    float restock_interval_ms; /* how often to restock */
    float time_accum_ms;       /* internal timer */
} RogueVendorRotation;

void rogue_vendor_reset(void);
/* Generate up to slots items from loot table; returns count generated */
int rogue_vendor_generate_inventory(int loot_table_index, int slots,
                                    const RogueGenerationContext* ctx, unsigned int* rng_state);
int rogue_vendor_item_count(void);
const RogueVendorItem* rogue_vendor_get(int index);
int rogue_vendor_price_formula(int item_def_index, int rarity);
/* Append vendor item (used by persistence restore). Returns new count or -1. */
int rogue_vendor_append(int def_index, int rarity, int price);

/* Phase 2.3–2.5: Template‑driven constrained generation
 * Uses vendor archetype inventory template (category & rarity weights) plus deterministic seed
 * (rogue_vendor_inventory_seed) to roll a constrained inventory obeying:
 *  - Uniqueness: no duplicate base item definitions
 *  - Rarity caps: at most 1 legendary (4), 2 epic (3), 4 rare (2) per refresh (tunable constants)
 *  - Guaranteed utility (consumable) slot: ensure >=1 consumable (ROGUE_ITEM_CONSUMABLE)
 *  - Material & recipe injection: inject at least one material item (category MATERIAL) and, if
 * crafting recipes are loaded, a recipe output item (treated as purchasable blueprint) when space
 * permits. Determinism: identical (world_seed,vendor_id,day_cycle,slots) -> identical ordered
 * inventory. Returns count produced (may be < slots if constraints + asset scarcity).
 */
int rogue_vendor_generate_constrained(const char* vendor_id, unsigned int world_seed, int day_cycle,
                                      int slots);

/* Rotation / Restock (10.3) */
void rogue_vendor_rotation_init(RogueVendorRotation* rot, float interval_ms);
int rogue_vendor_rotation_add_table(RogueVendorRotation* rot, int loot_table_index);
/* Advances timer; if interval elapsed performs restock (regenerates inventory) returning 1, else 0.
 */
int rogue_vendor_update_and_maybe_restock(RogueVendorRotation* rot, float dt_ms,
                                          const RogueGenerationContext* ctx, unsigned int* seed,
                                          int slots);
int rogue_vendor_current_table(const RogueVendorRotation* rot);

#endif /* ROGUE_VENDOR_H */
