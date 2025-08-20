#include "core/vendor_pricing.h"
#include "core/vendor_adaptive.h"
#include "core/vendor_econ_balance.h"
#include "core/econ_value.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Simple per-category demand tracking: EWMA of (sales - buybacks).
   sales increase demand_score, buybacks decrease. Demand scalar grows >1 if net sales recent.
*/
#ifndef ROGUE_VENDOR_DEMAND_CATEGORIES
#define ROGUE_VENDOR_DEMAND_CATEGORIES 16
#endif

static float g_demand_score[ROGUE_VENDOR_DEMAND_CATEGORIES];      /* short-term demand (recent sales vs buybacks) */
static float g_scarcity_score[ROGUE_VENDOR_DEMAND_CATEGORIES];    /* longer-term net outflow (positive => scarcity) */
static int g_inited = 0;

static void ensure_init(void){ if(g_inited) return; for(int i=0;i<ROGUE_VENDOR_DEMAND_CATEGORIES;i++){ g_demand_score[i]=0.0f; g_scarcity_score[i]=0.0f; } g_inited=1; }

void rogue_vendor_pricing_reset(void){ g_inited=0; ensure_init(); }

void rogue_vendor_pricing_record_sale(int category){
    ensure_init(); if(category<0||category>=ROGUE_VENDOR_DEMAND_CATEGORIES) return;
    /* Demand reacts quickly */
    g_demand_score[category] = g_demand_score[category]*0.85f + 1.0f;
    /* Scarcity accumulates more slowly with gentle decay (persists beyond immediate demand) */
    g_scarcity_score[category] = g_scarcity_score[category]*0.995f + 1.0f;
}
void rogue_vendor_pricing_record_buyback(int category){
    ensure_init(); if(category<0||category>=ROGUE_VENDOR_DEMAND_CATEGORIES) return;
    g_demand_score[category] = g_demand_score[category]*0.85f - 1.0f;
    g_scarcity_score[category] = g_scarcity_score[category]*0.995f - 1.0f;
}

float rogue_vendor_pricing_get_demand_scalar(int category){ ensure_init(); if(category<0||category>=ROGUE_VENDOR_DEMAND_CATEGORIES) return 1.0f; /* Map score to scalar: tanh mapped into [0.85,1.15] */ float s = g_demand_score[category]; float t = tanhf(s*0.15f); return 1.0f + t*0.15f; }
float rogue_vendor_pricing_get_scarcity_scalar(int category){
    ensure_init(); if(category<0||category>=ROGUE_VENDOR_DEMAND_CATEGORIES) return 1.0f;
    /* Map long-term scarcity score into roughly [0.9,1.2]. Positive score => scarcity => higher price. */
    float s = g_scarcity_score[category];
    /* Soft compression: scale then clamp */
    float scaled = s * 0.01f; if(scaled > 0.2f) scaled = 0.2f; if(scaled < -0.1f) scaled = -0.1f;
    return 1.0f + scaled; /* -0.1 .. +0.2 */
}

static float clampf(float v,float a,float b){ return v<a?a:(v>b?b:v); }

int rogue_vendor_compute_price(int vendor_def_index, int item_def_index, int rarity, int category,
                               int is_vendor_selling, int condition_pct,
                               int rep_tier_index, float negotiation_discount_pct){
    ensure_init();
    if(item_def_index<0) return 1;
    if(condition_pct<0) condition_pct=0; if(condition_pct>100) condition_pct=100;
    if(rarity<0) rarity=0; if(rarity>4) rarity=4;
    int base = rogue_econ_item_value(item_def_index, rarity, 0, 1.0f);
    if(base < 1) base = 1;
    /* Condition scalar (reuse econ_value logic concept 40%-100%) */
    float condition_scalar = 0.4f + 0.6f * (float)condition_pct / 100.0f;
    float price = (float)base * condition_scalar;
    /* Vendor policy margins */
    const RogueVendorDef* vd = (vendor_def_index>=0)? rogue_vendor_def_at(vendor_def_index) : NULL;
    const RoguePricePolicy* pol = NULL; if(vd && vd->price_policy_index>=0) pol = rogue_price_policy_at(vd->price_policy_index);
    int buy_margin=100, sell_margin=100; int rarity_mod=100, category_mod=100;
    if(pol){ buy_margin = pol->base_buy_margin; sell_margin = pol->base_sell_margin;
        if(rarity>=0 && rarity<5) rarity_mod = pol->rarity_mods[rarity];
        if(category>=0 && category<6) category_mod = pol->category_mods[category];
    }
    float margin_scalar = 1.0f;
    if(is_vendor_selling){
        margin_scalar = (float)buy_margin / 100.0f;
    } else {
        margin_scalar = (float)sell_margin / 100.0f;
    }
    float policy_scalar = margin_scalar * (float)rarity_mod / 100.0f * (float)category_mod / 100.0f;
    price *= policy_scalar;
    /* Reputation adjustments (buy discount / sell bonus) */
    if(rep_tier_index>=0){ const RogueRepTier* rt = rogue_rep_tier_at(rep_tier_index); if(rt){ if(is_vendor_selling){ price *= (100 - rt->buy_discount_pct)/100.0f; } else { price *= (100 + rt->sell_bonus_pct)/100.0f; } } }
    /* Negotiation discount applies only when vendor selling */
    if(is_vendor_selling && negotiation_discount_pct>0.0f){ if(negotiation_discount_pct>90.0f) negotiation_discount_pct=90.0f; price *= (100.0f - negotiation_discount_pct)/100.0f; }
    /* Demand & scarcity scalars */
    float demand_scalar = rogue_vendor_pricing_get_demand_scalar(category);
    float scarcity_scalar = rogue_vendor_pricing_get_scarcity_scalar(category);
    /* Adaptive exploit scalar (anti rapid flip) applies only when vendor buying items from player? We apply when vendor sells to player to damp exploitation; if flipping detected we raise vendor sell price slightly. */
    float exploit_scalar = 1.0f;
    if(is_vendor_selling){ exploit_scalar = rogue_vendor_adaptive_exploit_scalar(); }
     price *= demand_scalar * scarcity_scalar * exploit_scalar;
     /* Multi-vendor balancing (Phase 10): dynamic global margin & biome variance
         For now we approximate biome tags via vendor def's biome_tags field through registry if available. */
     float global_scalar = rogue_vendor_dynamic_margin_scalar();
     float biome_scalar = 1.0f; if(vd){ biome_scalar = rogue_vendor_biome_scalar(vd->biome_tags); }
     price *= global_scalar * biome_scalar;
     /* Feed observed price (pre-round) into inflation tracker */
     rogue_vendor_econ_balance_note_price((int)price);
    /* Clamp reasonable bounds */
    if(price < 1.0f) price = 1.0f;
    if(price > 1000000.0f) price = 1000000.0f; /* safety upper bound */
    int final_price = (int)floorf(price + 0.5f); if(final_price<1) final_price=1; return final_price;
}
