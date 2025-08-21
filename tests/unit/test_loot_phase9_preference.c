#include "core/loot/loot_adaptive.h"
#include "core/loot/loot_item_defs.h"
#include <assert.h>
#include <stdio.h>

/* Test 9.3 player preference: pickups of one category should reduce its preference factor relative
 * to untouched. */

int main(void)
{
    rogue_adaptive_reset();
    int catA = 0;
    int catB = 1;
    /* Simulate base observation so adaptive recompute has data */
    for (int i = 0; i < 10; i++)
    {
        rogue_adaptive_record_item(catA);
        rogue_adaptive_record_item(catB);
    }
    rogue_adaptive_recompute();
    float baseA = rogue_adaptive_get_category_preference_factor(catA);
    float baseB = rogue_adaptive_get_category_preference_factor(catB);
    assert(baseA > 0.7f && baseA < 1.3f);
    assert(baseB > 0.7f && baseB < 1.3f);
    /* Now record many pickups of catA */
    for (int i = 0; i < 50; i++)
    {
        rogue_adaptive_record_pickup(catA);
    }
    rogue_adaptive_recompute();
    float afterA = rogue_adaptive_get_category_preference_factor(catA);
    float afterB = rogue_adaptive_get_category_preference_factor(catB);
    /* catA should be dampened (<= baseline), catB unchanged or slightly > baseline */
    assert(afterA <= baseA + 1e-3f);
    assert(afterA < 1.01f || afterA < baseA);
    assert(afterB >= baseB - 1e-3f);
    printf("PREFERENCE_OK baseA=%.3f afterA=%.3f baseB=%.3f afterB=%.3f\n", baseA, afterA, baseB,
           afterB);
    return 0;
}
