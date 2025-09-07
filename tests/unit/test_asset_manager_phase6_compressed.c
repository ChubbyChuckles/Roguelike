#include "asset/asset_manager.h"
#include <stdio.h>
#include <string.h>

/* Phase 6 test: compressed texture fallback chain.
   We simulate presence/absence by calling internal substitution through public acquire path.
   Because actual filesystem probing occurs, we only verify that enabling the flag does not
   break normal acquisition for existing (likely uncompressed) test textures, and that repeated
   acquire returns same index (no duplicates) when preference toggled. */

static int failures = 0;
#define CHECK(cond, msg)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            failures++;                                                                            \
            fprintf(stderr, "FAIL: %s\n", msg);                                                    \
        }                                                                                          \
    } while (0)

int main(void)
{
    /* Initialize headless (renderer NULL) so actual loads may fail; we only test path logic. */
    rogue_asset_manager_init(NULL);
    rogue_asset_manager_set_prefer_compressed_textures(0);
    int a = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_a.png");
    int b = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_b.png");
    CHECK(a >= 0 && b >= 0 && a != b, "basic acquire indices distinct");
    /* Enable preference; acquiring again should yield identical indices (no duplicate records) */
    rogue_asset_manager_set_prefer_compressed_textures(1);
    int a2 = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_a.png");
    int b2 = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_b.png");
    CHECK(a == a2 && b == b2, "compressed preference does not duplicate records");
    /* Disable again and re-acquire */
    rogue_asset_manager_set_prefer_compressed_textures(0);
    int a3 = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_a.png");
    CHECK(a3 == a, "re-acquire stable after toggling preference");
    rogue_asset_manager_shutdown();
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
