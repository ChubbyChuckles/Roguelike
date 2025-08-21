/* Vendor System Phase 2.1/2.2
 * Inventory templates (per vendor archetype) and deterministic seed composition.
 * Provides category weight & rarity tier weight guidance for upcoming constrained
 * roll algorithm (Phase 2.3) along with a seed derivation function:
 *   world_seed ^ fnv32(vendor_id) ^ day_cycle
 */
#ifndef ROGUE_VENDOR_INVENTORY_TEMPLATES_H
#define ROGUE_VENDOR_INVENTORY_TEMPLATES_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "core/loot/loot_item_defs.h"

#define ROGUE_MAX_VENDOR_INV_TEMPLATES 32

    typedef struct RogueVendorInventoryTemplate
    {
        char archetype[32];
        int category_weights[ROGUE_ITEM__COUNT]; /* weighting for selection / relative frequency */
        int rarity_weights[5];                   /* weight per rarity tier 0..4 */
    } RogueVendorInventoryTemplate;

    /* Load inventory templates from JSON (assets/vendors/inventory_templates.json). Returns 1 on
     * success (>0 templates). */
    int rogue_vendor_inventory_templates_load(void);
    int rogue_vendor_inventory_template_count(void);
    const RogueVendorInventoryTemplate* rogue_vendor_inventory_template_at(int idx);
    const RogueVendorInventoryTemplate* rogue_vendor_inventory_template_find(const char* archetype);

    /* Deterministic vendor inventory seed composition (Phase 2.2):
       Combines world_seed, vendor_id string (hashed), and day_cycle integer via xor mixing. */
    unsigned int rogue_vendor_inventory_seed(unsigned int world_seed, const char* vendor_id,
                                             int day_cycle);

#ifdef __cplusplus
}
#endif
#endif /* ROGUE_VENDOR_INVENTORY_TEMPLATES_H */
