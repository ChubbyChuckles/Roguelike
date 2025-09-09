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
#include "../loot/loot_item_defs.h"
#include "vendor_registry.h"

#ifdef __cplusplus
extern "C"
{
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
       negotiation_discount_pct: discount percentage from negotiation (0 if none) applied only to
       vendor selling.
    */
    int rogue_vendor_compute_price(int vendor_def_index, int item_def_index, int rarity,
                                   int category, int is_vendor_selling, int condition_pct,
                                   int rep_tier_index, float negotiation_discount_pct);

    /* Phase 12: hash of pricing modifier state for snapshot hashing */
    unsigned int rogue_vendor_price_modifiers_hash(void);

    /* Phase 13.1 persistence helpers: export/import demand & scarcity arrays.
       Returns number of categories exported/imported. 'cap' must be >= returned count. */
    int rogue_vendor_pricing_export(float* out_demand, float* out_scarcity, int cap);
    int rogue_vendor_pricing_import(const float* demand, const float* scarcity, int count);

    /* Phase 16.2: Pricing breakdown for UI tooltips and telemetry. */
    typedef struct RogueVendorPriceBreakdown
    {
        int base_value;           /* econ base before scalars */
        float condition_scalar;   /* 0.4..1.0 based on durability */
        float policy_scalar;      /* vendor margin * rarity * category */
        float rep_scalar;         /* reputation tier scalar (buy or sell) */
        float negotiation_scalar; /* negotiation discount (<=1 when vendor selling) */
        float demand_scalar;      /* short-term EWMA sales vs buybacks */
        float scarcity_scalar;    /* long-term scarcity */
        float exploit_scalar;     /* adaptive exploit guard (Phase 9.3) */
        float security_scalar;    /* security spree guard (Phase 15) */
        float global_scalar;      /* inflation rebalancer (Phase 10.2) */
        float biome_scalar;       /* regional variance (Phase 10.3) */
        int final_price;          /* rounded final integer price */
    } RogueVendorPriceBreakdown;

    /* Compute price and fill breakdown. Mirrors rogue_vendor_compute_price semantics. */
    int rogue_vendor_compute_price_with_breakdown(int vendor_def_index, int item_def_index,
                                                  int rarity, int category, int is_vendor_selling,
                                                  int condition_pct, int rep_tier_index,
                                                  float negotiation_discount_pct,
                                                  RogueVendorPriceBreakdown* out);

    /* Format a human-readable multi-line tooltip from a breakdown. Returns bytes written. */
    int rogue_vendor_format_price_tooltip(const RogueVendorPriceBreakdown* br, char* buf, int cap);

    /* Test helper (Phase 16.2): compose final price from explicit components without
       querying registries. Useful for deterministic unit tests. Fills out (copying inputs)
       and computes final_price with rounding/clamp rules. */
    int rogue_vendor_compose_price_testonly(int base_value, float condition_scalar,
                                            float policy_scalar, float rep_scalar,
                                            float negotiation_scalar, float demand_scalar,
                                            float scarcity_scalar, float exploit_scalar,
                                            float security_scalar, float global_scalar,
                                            float biome_scalar, RogueVendorPriceBreakdown* out);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_VENDOR_PRICING_H */
