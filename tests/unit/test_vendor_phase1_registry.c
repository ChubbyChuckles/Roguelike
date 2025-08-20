/* Vendor System Phase 1 (1.1-1.3) registry & parsing tests */
#include "core/vendor_registry.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <string.h>

int main(void){
    if(!rogue_vendor_registry_load_all()){ fprintf(stderr,"VENDOR_P1_FAIL load_all\n"); return 5; }
    if(rogue_vendor_def_count() <= 0){ fprintf(stderr,"VENDOR_P1_FAIL vendors count\n"); return 6; }
    if(rogue_price_policy_count() <= 0){ fprintf(stderr,"VENDOR_P1_FAIL policies count\n"); return 7; }
    if(rogue_rep_tier_count() <= 0){ fprintf(stderr,"VENDOR_P1_FAIL rep tiers count\n"); return 8; }
    const RogueVendorDef* blacksmith = rogue_vendor_def_find("v_blacksmith"); if(!blacksmith){ fprintf(stderr,"VENDOR_P1_FAIL find blacksmith\n"); return 9; }
    if(blacksmith->price_policy_index < 0){ fprintf(stderr,"VENDOR_P1_FAIL policy index resolve\n"); return 10; }
    const RoguePricePolicy* pol = rogue_price_policy_at(blacksmith->price_policy_index); if(!pol){ fprintf(stderr,"VENDOR_P1_FAIL policy deref\n"); return 11; }
    if(pol->base_buy_margin <= 0 || pol->base_sell_margin <= 0){ fprintf(stderr,"VENDOR_P1_FAIL policy margins\n"); return 12; }
    /* Reputation tier ordering by rep_min ascending */
    int last = -1; for(int i=0;i<rogue_rep_tier_count();i++){ const RogueRepTier* rt = rogue_rep_tier_at(i); if(rt->rep_min < last){ fprintf(stderr,"VENDOR_P1_FAIL rep tier order\n"); return 13; } last = rt->rep_min; }
    printf("VENDOR_PHASE1_REGISTRY_OK vendors=%d policies=%d rep=%d\n", rogue_vendor_def_count(), rogue_price_policy_count(), rogue_rep_tier_count());
    return 0;
}
