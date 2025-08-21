#include <assert.h>
#include <stdio.h>
#include "core/progression/progression_attributes.h"

static void test_spend_and_respec(void){
    RogueAttributeState st; rogue_attr_state_init(&st,5,5,15,5);
    /* grant starting pool */
    rogue_attr_grant_points(10);
    int before_unspent = rogue_attr_unspent_points();
    assert(before_unspent>=10);
    assert(rogue_attr_spend(&st,'S')==0);
    assert(rogue_attr_spend(&st,'D')==0);
    assert(st.strength==6 && st.dexterity==6);
    unsigned long long fp1 = rogue_attr_fingerprint(&st);
    st.respec_tokens = 2;
    assert(rogue_attr_respec(&st,'S')==0); /* refund strength */
    assert(st.strength==5);
    unsigned long long fp2 = rogue_attr_fingerprint(&st);
    assert(fp1!=fp2); /* fingerprint changed */
    /* Ensure unspent increased by 1 */
    int after_unspent = rogue_attr_unspent_points();
    assert(after_unspent == before_unspent - 1); /* spent 2, refunded 1 */
}

int main(void){
    test_spend_and_respec();
    printf("progression_phase2_attributes: OK\n");
    return 0;
}
