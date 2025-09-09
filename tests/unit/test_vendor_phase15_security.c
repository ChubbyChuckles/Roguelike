#include "../../src/core/vendor/vendor_security.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_spree_scalar_bounds(void)
{
    rogue_vendor_security_reset();
    /* No purchases => scalar 1.0 */
    assert(rogue_vendor_security_spree_scalar() == 1.0f);
    /* Below threshold (<=10 in 10s) => still 1.0 */
    for (unsigned i = 0, t = 1000; i < 10; ++i, t += 500)
    {
        rogue_vendor_security_note_purchase(t);
    }
    float s1 = rogue_vendor_security_spree_scalar();
    assert(s1 >= 0.999f && s1 <= 1.001f);
    /* Exceed threshold toward cap (20) within 10s => rise up to +5% */
    for (unsigned i = 0, t = 2000; i < 12; ++i, t += 400)
    {
        rogue_vendor_security_note_purchase(t);
    }
    float s2 = rogue_vendor_security_spree_scalar();
    assert(s2 > 1.0f && s2 <= 1.05f);
    /* Ensure cap respected: many more within window should not exceed 1.05 */
    for (unsigned i = 0, t = 2500; i < 40; ++i, t += 150)
    {
        rogue_vendor_security_note_purchase(t);
    }
    float s3 = rogue_vendor_security_spree_scalar();
    assert(s3 <= 1.05f + 1e-3f);
}

static void test_negotiation_spam_lockout(void)
{
    rogue_vendor_security_reset();
    int vidx = 0;
    unsigned now = 0;
    /* First 5 attempts within window should be allowed */
    for (int i = 0; i < 5; ++i)
    {
        int ok = rogue_vendor_security_negotiation_allowed(vidx, now);
        assert(ok == 1);
        now += 1000;
    }
    /* The 6th+ within 10s should trigger extended lockout (20s) */
    int ok6 = rogue_vendor_security_negotiation_allowed(vidx, now);
    assert(ok6 == 0);
    unsigned rem = rogue_vendor_security_negotiation_lockout_remaining(vidx, now);
    assert(rem >= 19000u && rem <= 20000u);
    /* During lockout, attempts must be blocked */
    now += 5000u;
    assert(rogue_vendor_security_negotiation_allowed(vidx, now) == 0);
    /* After lockout expires, allowed again */
    now += 20000u;
    assert(rogue_vendor_security_negotiation_allowed(vidx, now) == 1);
}

int main(void)
{
    test_spree_scalar_bounds();
    test_negotiation_spam_lockout();
    printf("VENDOR_PHASE15_SECURITY_OK\n");
    return 0;
}
