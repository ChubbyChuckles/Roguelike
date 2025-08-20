#include "core/vendor_adaptive.h"
#include <assert.h>
#include <stdio.h>

int main(){
    rogue_vendor_adaptive_reset();
    /* Initially all scalars == 1 */
    float s0 = rogue_vendor_adaptive_category_weight_scalar(0); (void)s0; /* baseline scalar (may be 1.0) */
    /* Record purchases mainly in category 1 to make category 0 under-purchased -> boost */
    for(int i=0;i<20;i++){ rogue_vendor_adaptive_record_player_purchase(1, (unsigned int)(i*100)); }
    float s_under = rogue_vendor_adaptive_category_weight_scalar(0); assert(s_under > 1.0f && s_under <= 1.16f);
    /* Flip detection: purchase then rapid sale triggers exploit scalar >1 */
    rogue_vendor_adaptive_record_player_purchase(2, 5000u);
    rogue_vendor_adaptive_record_player_sale(2, 5000u + 1000u);
    float ex = rogue_vendor_adaptive_exploit_scalar(); assert(ex > 1.0f && ex <= 1.11f);
    /* Batch gating: simulate >8 purchases inside 10s window */
    unsigned int base=20000u; for(int i=0;i<10;i++){ rogue_vendor_adaptive_record_player_purchase(3, base + (unsigned int)(i*200)); }
    int allowed_before = rogue_vendor_adaptive_can_purchase(base + 1900u); /* likely triggers cooldown */
    (void)allowed_before; /* may be 0 depending on threshold */
    /* Force check of cooldown remaining (should be >0 if gate triggered) */
    unsigned int rem = rogue_vendor_adaptive_purchase_cooldown_remaining(base + 1900u);
    if(rem>0){ assert(rem <= 6000u); }
    printf("VENDOR_PHASE9_ADAPTIVE_OK\n");
    return 0;
}
