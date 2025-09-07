#include "asset/asset_manager.h"
#include <stdio.h>
#include <string.h>

/* Phase 6 test: atlas builder UV integrity.
   In headless (NULL renderer) environment atlas build should fail (returns -1) so we skip.
   If renderer available (future harness), we would validate UV partitioning invariants.
*/

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
    rogue_asset_manager_init(NULL);
    int idxA = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_a.png");
    int idxB = rogue_asset_manager_acquire_texture("assets/art/example_pack/test_tex_b.png");
    int indices[2] = {idxA, idxB};
    RogueAtlasUV uvs[2];
    int atlas = rogue_asset_manager_build_atlas_horizontal(indices, 2, uvs, 2);
    /* In headless mode expect failure */
    CHECK(atlas == -1, "atlas build should fail headless");
    rogue_asset_manager_shutdown();
    if (failures)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
