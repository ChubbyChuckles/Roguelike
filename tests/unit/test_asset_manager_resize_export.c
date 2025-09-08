#include "../../src/asset/asset_manager.h"
#include <assert.h>
#include <stdio.h>

/* Simple smoke tests for resize/export APIs in headless-safe manner.
   These run without a renderer so they should fail gracefully (return -1/0) and not crash. */
int main(void)
{
    assert(rogue_asset_manager_init(NULL) && "Init without renderer should succeed headless");
    int tex = rogue_asset_manager_acquire_texture("assets/placeholder.png");
    assert(tex >= 0);
    /* Headless resize must fail (no renderer) */
    int rvar = rogue_asset_manager_resize_texture_variant(tex, 32, 32, 0);
    assert(rvar == -1);
    int rinp = rogue_asset_manager_resize_texture_variant(tex, 16, 16, 1);
    assert(rinp == -1);
    int ex = rogue_asset_manager_export_texture_bmp(tex, "build/test_export_headless.bmp");
    assert(ex == 0);
    rogue_asset_manager_release_texture(tex);
    rogue_asset_manager_shutdown();
    printf("asset_manager_resize_export_headless OK\n");
    return 0;
}
