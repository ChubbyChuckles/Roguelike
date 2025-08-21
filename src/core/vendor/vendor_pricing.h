#ifndef ROGUE_VENDOR_PRICING_H
#define ROGUE_VENDOR_PRICING_H
/* Vendor Pricing Engine (Phase 3)
   Composes final buy/sell prices from:
     1. Base economic value (econ_value)
     2. Condition scalar (durability / condition percentage)
     3. Vendor price policy margins + rarity & category percent modifiers
     4. Reputation tier discount (buy) or sell bonus (sell)
     5. Negotiation discount (placeholder hook; Phase 4 extends)
     6. Dynamic demand scalar (EWMA of recent sales vs buybacks)
     7. Scarcity scalar (net flow imbalance long-term)
     8. Clamp & rounding to integer coins (>=1)

   All demand/scarcity tracking is global per item category (lightweight, deterministic).
*/
#include "core/loot/loot_item_defs.h"
#include "core/vendor/vendor_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

void rogue_vendor_pricing_reset(void);

/* Record that the vendor SOLD an item of category (player purchased). */
void rogue_vendor_pricing_record_sale(int category);
/* Record that the vendor BOUGHT an item back from player (buyback). */
void rogue_vendor_pricing_record_buyback(int category);

float rogue_vendor_pricing_get_demand_scalar(int category);
float rogue_vendor_pricing_get_scarcity_scalar(int category);

/* Compute final price.
   vendor_def_index: index into vendor registry (for policy), or -1 for default neutral margins.
   item_def_index / rarity / category used for base & modifiers.
   is_vendor_selling: 1 => price player pays; 0 => price vendor pays player for selling item.
   condition_pct: 0-100 condition (durability) percentage (applied as linear blend 40%-100%).
   rep_tier_index: reputation tier index or -1 if none.
   negotiation_discount_pct: discount percentage from negotiation (0 if none) applied only to vendor selling.
*/
int rogue_vendor_compute_price(int vendor_def_index, int item_def_index, int rarity, int category,
                               int is_vendor_selling, int condition_pct,
                               int rep_tier_index, float negotiation_discount_pct);

/* Phase 12: hash of pricing modifier state for snapshot hashing */
unsigned int rogue_vendor_price_modifiers_hash(void);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_PRICING_H */
