#include "core/vendor/vendor_registry.h"
#include <stdio.h>
#include <string.h>

/* Phase 1.4 + remaining 1.5 tests: negotiation rule load + collision / padding resilience */

static int simulate_collision_duplicate_rule(void){
    /* We cannot easily inject alternate file without filesystem write override; rely on existing loader.
       Instead verify there are no duplicate IDs among loaded negotiation rules. */
    int n = rogue_negotiation_rule_count();
    for(int i=0;i<n;i++){
        const RogueNegotiationRule* a = rogue_negotiation_rule_at(i);
        for(int j=i+1;j<n;j++){
            const RogueNegotiationRule* b = rogue_negotiation_rule_at(j);
            if(strcmp(a->id,b->id)==0){ printf("NEG_RULE_COLLISION id=%s indexes=%d,%d\n", a->id,i,j); return 0; }
        }
    }
    return 1;
}

static int validate_ranges(void){
    int n=rogue_negotiation_rule_count();
    if(n<=0){ printf("NEG_RULE_FAIL none\n"); return 0; }
    for(int i=0;i<n;i++){
        const RogueNegotiationRule* r=rogue_negotiation_rule_at(i);
        if(r->min_roll < 0 || r->discount_min_pct <0 || r->discount_max_pct < r->discount_min_pct){
            printf("NEG_RULE_FAIL range id=%s\n", r->id); return 0; }
        if(strlen(r->id)==0){ printf("NEG_RULE_FAIL empty_id\n"); return 0; }
    }
    return 1;
}

int main(void){
    if(!rogue_vendor_registry_load_all()){ printf("NEG_RULE_FAIL load_all\n"); return 1; }
    if(!simulate_collision_duplicate_rule()) return 2;
    if(!validate_ranges()) return 3;
    printf("VENDOR_PHASE1_NEGOTIATION_OK rules=%d rep=%d vendors=%d policies=%d\n", rogue_negotiation_rule_count(), rogue_rep_tier_count(), rogue_vendor_def_count(), rogue_price_policy_count());
    return 0;
}
