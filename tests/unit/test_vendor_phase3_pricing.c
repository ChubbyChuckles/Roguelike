/* Vendor System Phase 3.1–3.5 Pricing Engine Tests
   Validates pipeline ordering, determinism, floor/ceiling, demand response, and reputation/negotiation effects.
*/
#include "core/vendor/vendor_pricing.h"
#include "core/vendor/vendor_registry.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

static int ensure_items(void){
    if(rogue_item_defs_count()>0) return 1;
    /* Try legacy cfg directory load first */
    char p[256]; if(rogue_find_asset_path("items/swords.cfg", p, sizeof p)){
        for(int i=(int)strlen(p)-1;i>=0;i--){ if(p[i]=='/'||p[i]=='\\'){ p[i]='\0'; break; } }
        if(rogue_item_defs_load_directory(p)>0) return 1;
    }
    /* Fallback: JSON (Phase 16 tooling) – try nested then root */
    char pj[256]; if(!rogue_find_asset_path("items/items.json", pj, sizeof pj)){
        rogue_find_asset_path("items.json", pj, sizeof pj);
    }
    if(pj[0]){
        if(rogue_item_defs_load_from_json(pj)>0) return 1;
    }
    return 0;
}

int main(void){
    printf("VENDOR_P3_DEBUG start\n"); fflush(stdout);
    if(!ensure_items()){ printf("VENDOR_P3_FAIL load items\n"); return 1; }
    if(!rogue_vendor_registry_load_all()){ printf("VENDOR_P3_FAIL registry load\n"); return 2; }
    rogue_vendor_pricing_reset();
    /* Pick a known vendor & item */
    const RogueVendorDef* vd = rogue_vendor_def_find("blacksmith_standard"); if(!vd){ printf("VENDOR_P3_FAIL find vendor\n"); return 3; }
    int weapon_def=-1; for(int i=0;i<rogue_item_defs_count();i++){ const RogueItemDef* d=rogue_item_def_at(i); if(d && d->category==ROGUE_ITEM_WEAPON){ weapon_def=i; break; } }
    if(weapon_def<0){ printf("VENDOR_P3_FAIL no weapon\n"); return 4; }
    int base_price = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    if(base_price < 1){ printf("VENDOR_P3_FAIL base price<1 %d\n", base_price); return 5; }
    /* Demand increase after sales should increase vendor selling price (monotonic) */
    int p0 = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    rogue_vendor_pricing_record_sale(ROGUE_ITEM_WEAPON);
    int p1 = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    if(p1 < p0){ printf("VENDOR_P3_FAIL demand not increasing price %d %d\n", p0,p1); return 6; }
    /* Buyback reduces demand and lowers price */
    rogue_vendor_pricing_record_buyback(ROGUE_ITEM_WEAPON);
    int p2 = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    if(p2 > p1){ printf("VENDOR_P3_FAIL buyback not reducing price %d %d\n", p1,p2); return 7; }
    /* Condition scaling: 50% condition should be lower */
    int p_full = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    int p_half = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 50, -1, 0.0f);
    if(p_half >= p_full){ printf("VENDOR_P3_FAIL condition scalar %d %d\n", p_full,p_half); return 8; }
    /* Reputation discount check if any rep tiers exist */
    /* Use first tier that has a non-zero discount if available */
    int rep_tier = -1; for(int rt=0; rt<rogue_rep_tier_count(); rt++){ const RogueRepTier* t = rogue_rep_tier_at(rt); if(t && t->buy_discount_pct>0){ rep_tier = rt; break; } }
    if(rep_tier>=0){ int p_rep = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, rep_tier, 0.0f); if(p_rep >= p_full){ printf("VENDOR_P3_FAIL rep discount absent %d %d\n", p_full,p_rep); return 9; } }
    /* Negotiation discount */
    int p_neg = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 10.0f);
    if(p_neg >= p_full){ printf("VENDOR_P3_FAIL negotiation discount absent %d %d\n", p_full,p_neg); return 10; }
    /* Sell (vendor buying) price should be lower than buy price normally */
    int p_buy = p_full;
    int p_sell = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 0, 100, -1, 0.0f);
    if(p_sell >= p_buy){ printf("VENDOR_P3_FAIL sell vs buy margins %d %d\n", p_buy,p_sell); return 11; }
    /* Scarcity accumulation: simulate many sales without buybacks => price should trend upward vs base */
    int before_scarcity = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    for(int i=0;i<120;i++){ rogue_vendor_pricing_record_sale(ROGUE_ITEM_WEAPON); }
    int after_scarcity = rogue_vendor_compute_price((int)(vd - rogue_vendor_def_at(0)), weapon_def, 0, ROGUE_ITEM_WEAPON, 1, 100, -1, 0.0f);
    if(after_scarcity <= before_scarcity){ printf("VENDOR_P3_FAIL scarcity no increase %d %d\n", before_scarcity, after_scarcity); return 12; }
    printf("VENDOR_PHASE3_PRICING_OK base=%d demand_up=%d demand_down=%d half=%d sell=%d negotiation=%d scarcity=%d->%d\n", p0,p1,p2,p_half,p_sell,p_neg,before_scarcity,after_scarcity);
    return 0;
}
