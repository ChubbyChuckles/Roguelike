#include "../../src/game/buffs.h"
#include <stdio.h>
#include <string.h>

/* Minimal time helper: we drive time manually via now_ms params */

static int expire_calls = 0;
static void on_expire(RogueBuffType type, int magnitude)
{
    (void) type;
    (void) magnitude;
    expire_calls++;
}

int main(void)
{
    rogue_buffs_init();
    rogue_buffs_set_dampening(0.0);        /* don't block rapid applies in tests */
    rogue_buffs_set_dr_window_ms(10000.0); /* 10s window */
    rogue_buffs_reset_dr_state();
    rogue_buffs_set_on_expire(on_expire);

    double t = 1000.0;

    /* 1) DR for stun: durations should scale 1.0, 0.5, 0.25, 0.0 within window */
    RogueBuffHandle h1 =
        rogue_buffs_apply_h(ROGUE_BUFF_CC_STUN, 1, 4000.0, t, ROGUE_BUFF_STACK_REFRESH, 1);
    if (!h1)
    {
        printf("FAIL: no handle on first stun\n");
        return 1;
    }
    RogueBuff b1;
    rogue_buffs_query_h(h1, &b1);
    if (b1.end_ms - t < 3999.0 || b1.end_ms - t > 4001.0)
    {
        printf("FAIL: stun1 dur %.2f\n", b1.end_ms - t);
        return 1;
    }

    RogueBuffHandle h2 =
        rogue_buffs_apply_h(ROGUE_BUFF_CC_STUN, 1, 4000.0, t + 10.0, ROGUE_BUFF_STACK_REFRESH, 1);
    RogueBuff b2;
    rogue_buffs_query_h(h2, &b2);
    if (b2.end_ms - (t + 10.0) < 1999.0 || b2.end_ms - (t + 10.0) > 2001.0)
    {
        printf("FAIL: stun2 dur %.2f\n", b2.end_ms - (t + 10.0));
        return 1;
    }

    RogueBuffHandle h3 =
        rogue_buffs_apply_h(ROGUE_BUFF_CC_STUN, 1, 4000.0, t + 20.0, ROGUE_BUFF_STACK_REFRESH, 1);
    RogueBuff b3;
    rogue_buffs_query_h(h3, &b3);
    if (b3.end_ms - (t + 20.0) < 999.0 || b3.end_ms - (t + 20.0) > 1001.0)
    {
        printf("FAIL: stun3 dur %.2f\n", b3.end_ms - (t + 20.0));
        return 1;
    }

    RogueBuffHandle h4 =
        rogue_buffs_apply_h(ROGUE_BUFF_CC_STUN, 1, 4000.0, t + 30.0, ROGUE_BUFF_STACK_REFRESH, 1);
    if (h4)
    {
        printf("FAIL: stun4 should be zero duration under DR (handle should be invalid)\n");
        return 1;
    }

    /* 2) DR decay: after window passes, next stun should be full duration again */
    t += 10000.0 + 1.0; /* move past window */
    RogueBuffHandle h5 =
        rogue_buffs_apply_h(ROGUE_BUFF_CC_STUN, 1, 3000.0, t, ROGUE_BUFF_STACK_REFRESH, 1);
    if (!h5)
    {
        printf("FAIL: no handle after DR window\n");
        return 1;
    }
    RogueBuff b5;
    rogue_buffs_query_h(h5, &b5);
    if (b5.end_ms - t < 2999.0 || b5.end_ms - t > 3001.0)
    {
        printf("FAIL: stun after window dur %.2f\n", b5.end_ms - t);
        return 1;
    }

    /* 3) Expiration callback fires on natural expiry and manual remove */
    int before = expire_calls;
    rogue_buffs_update(b1.end_ms + 0.1); /* expire first chain */
    if (expire_calls <= before)
    {
        printf("FAIL: no expire callback on natural expiry\n");
        return 1;
    }

    before = expire_calls;
    (void) rogue_buffs_remove_h(h5, b5.end_ms - 1.0);
    if (expire_calls <= before)
    {
        printf("FAIL: no expire callback on manual remove\n");
        return 1;
    }

    /* 4) Handle reuse safety: removed handle should be invalid for query */
    RogueBuff tmp;
    if (rogue_buffs_query_h(h5, &tmp))
    {
        printf("FAIL: query succeeded on freed handle\n");
        return 1;
    }

    printf("BUFFS_PHASE4_DR_HANDLES_OK\n");
    return 0;
}
