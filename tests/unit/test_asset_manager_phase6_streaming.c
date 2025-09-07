/* test_asset_manager_phase6_streaming.c - validates Phase 6 streaming queue + metrics */
#include "../../src/asset/asset_manager.h"
#include <stdio.h>

static int check(int cond, const char* msg)
{
    if (!cond)
    {
        printf("FAIL: %s\n", msg);
        return 0;
    }
    return 1;
}

int main(void)
{
    if (!rogue_asset_manager_init(NULL))
    {
        printf("FAIL: init\n");
        return 1;
    }
    /* Enable streaming; enqueue a couple of textures (may not exist, that's okay). */
    rogue_asset_manager_set_streaming_enabled(true);
    int a = rogue_asset_manager_enqueue_texture_stream("assets/placeholder.png");
    int b = rogue_asset_manager_enqueue_texture_stream("assets/missing_stream_test.png");
    if (!check(a >= 0, "enqueue placeholder"))
        return 1;
    if (!check(b >= 0, "enqueue missing texture slot allocated"))
        return 1;
    int depth_before = rogue_asset_manager_stream_queue_depth();
    if (!check(depth_before >= 1, "queue depth positive"))
        return 1;
    /* Process the queue fully (headless: loads may be skipped but queue should drain). */
    int loaded = rogue_asset_manager_stream_step(0);
    (void) loaded; /* count optional headless */
    int depth_after = rogue_asset_manager_stream_queue_depth();
    if (!check(depth_after == 0, "queue drained after step"))
        return 1;
    RogueAssetMetrics m;
    rogue_asset_manager_get_metrics(&m);
    if (!check(m.stream_queue_depth == 0, "metrics queue depth 0"))
        return 1;
    /* streamed count may be 0 in headless; just ensure field readable. */
    (void) m.stream_loaded_count;
    printf("OK test_asset_manager_phase6_streaming\n");
    return 0;
}
