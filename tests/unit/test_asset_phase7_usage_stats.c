/* test_asset_phase7_usage_stats.c - Phase 7 extended usage analytics */
#include "asset/asset_manager.h"
#include "asset/asset_validation.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(rogue_asset_manager_init(NULL));
    int a = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_a.png");
    int b = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_b.png");
    assert(a >= 0 && b >= 0);
    RogueAssetUsageStats s1 = rogue_asset_usage_stats();
    assert(s1.texture_records >= 2);
    /* Simulate reload path (headless may not reload, so just manually note) */
    rogue_asset_usage_note_reload();
    RogueAssetUsageStats s2 = rogue_asset_usage_stats();
    assert(s2.reloads_detected >= s1.reloads_detected);
    assert(s2.peak_texture_records >= s2.texture_records);
    rogue_asset_manager_shutdown();
    rogue_asset_validation_shutdown();
    printf("test_asset_phase7_usage_stats OK\n");
    return 0;
}
