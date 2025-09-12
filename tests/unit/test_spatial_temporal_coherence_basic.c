#include "game/spatial_acceleration.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    RogueTemporalCoherenceCache cache;
    rogue_temporal_cache_init(&cache, 5.0f); /* 5px sep threshold */

    /* Two entities with small separation increasing slowly */
    uint32_t A = 10, B = 20;
    RogueVec2 v = {0.5f, 0.0f};

    /* First touch: sep^2 = 4, not colliding */
    int idx = rogue_temporal_cache_touch(&cache, A, B, 4.0f, v, 0);
    assert(idx >= 0);

    /* Predict with slightly larger separation and same small velocity */
    int pred = rogue_temporal_cache_predict_skip(&cache, A, B, 4.5f, v);
    assert(pred == 1);

    /* Update as not colliding again; ensure skip flag resets on touch */
    idx = rogue_temporal_cache_touch(&cache, A, B, 4.5f, v, 0);
    assert(idx >= 0);

    /* If separation decreases or speed is large, prediction should not trigger */
    RogueVec2 fast = {10.0f, 0.0f};
    pred = rogue_temporal_cache_predict_skip(&cache, A, B, 4.0f, fast);
    assert(pred == 0);

    /* Different pair hash should not collide with original entry */
    uint32_t C = 30, D = 40;
    idx = rogue_temporal_cache_touch(&cache, C, D, 1.0f, v, 0);
    assert(idx >= 0);
    pred = rogue_temporal_cache_predict_skip(&cache, C, D, 1.1f, v);
    assert(pred == 1);

    printf("spatial_temporal_coherence_basic: PASS\n");
    return 0;
}
