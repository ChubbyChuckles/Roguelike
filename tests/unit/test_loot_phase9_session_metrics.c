#include "core/app_state.h"
#include "core/metrics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Test 9.5: session metrics items/hour & rarity/hour calculation.
   We bypass full loot pipeline and call record functions directly to ensure deterministic counts.
   To simulate elapsed time, we manually adjust the session_start_seconds backward via extern g_app.
 */

extern struct RogueAppState g_app; /* declared in app_state.h */

int main(void)
{
    rogue_metrics_reset();
    /* Simulate 90 seconds elapsed by moving start back */
    g_app.session_start_seconds -= 90.0; /* 1.5 minutes */
    /* Record 30 drops: rarities distributed 10 common (0), 8 uncommon (1), 6 rare (2), 4 epic (3),
     * 2 legendary (4) */
    for (int i = 0; i < 10; i++)
        rogue_metrics_record_drop(0);
    for (int i = 0; i < 8; i++)
        rogue_metrics_record_drop(1);
    for (int i = 0; i < 6; i++)
        rogue_metrics_record_drop(2);
    for (int i = 0; i < 4; i++)
        rogue_metrics_record_drop(3);
    for (int i = 0; i < 2; i++)
        rogue_metrics_record_drop(4);
    /* Record 12 pickups (subset) */
    for (int i = 0; i < 12; i++)
        rogue_metrics_record_pickup(i % 5);
    double iph = 0.0;
    double rph[5];
    memset(rph, 0, sizeof rph);
    rogue_metrics_rates(&iph, rph);
    /* 30 drops in 1.5 minutes = 20 per minute = 1200 per hour */
    assert(iph > 1100.0 && iph < 1300.0);
    /* Common per hour: 10 in 1.5 min => (10/1.5)*60 = 400 */
    assert(rph[0] > 350.0 && rph[0] < 450.0);
    /* Legendary per hour: 2 in 1.5 min => (2/1.5)*60 = 80 */
    assert(rph[4] > 60.0 && rph[4] < 100.0);
    printf("SESSION_METRICS_OK iph=%.2f common_ph=%.2f legendary_ph=%.2f picked=%u\n", iph, rph[0],
           rph[4], g_app.session_items_picked);
    return 0;
}
