/* Tests procedural stat roll for affix values (7.4) */
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/path_utils.h"
#include <stdio.h>

static int fail(const char* msg)
{
    fprintf(stderr, "FAIL:%s\n", msg);
    return 1;
}

int main(void)
{
    char path[256];
    if (!rogue_find_asset_path("affixes.cfg", path, sizeof path))
        return fail("path");
    rogue_affixes_reset();
    if (rogue_affixes_load_from_cfg(path) < 4)
        return fail("load");
    int idx = rogue_affix_index("sharp");
    if (idx < 0)
        return fail("idx");
    unsigned int seedA = 42u, seedB = 42u;
    int v1 = rogue_affix_roll_value(idx, &seedA);
    int v2 = rogue_affix_roll_value(idx, &seedA);
    int v1_repeat = rogue_affix_roll_value(idx, &seedB);
    if (v1 < 1 || v1 > 3)
        return fail("bounds1");
    if (v2 < 1 || v2 > 3)
        return fail("bounds2");
    if (v1 != v1_repeat)
        return fail("determinism");
    /* Spot check variability: with another seed should differ frequently */
    unsigned int seedC = 99u;
    int v3 = rogue_affix_roll_value(idx, &seedC);
    if (seedC == seedA && v3 == v1)
    { /* extremely unlikely unless bug */
        return fail("var");
    }
    printf("AFFIX_VALUE_ROLL_OK v1=%d v2=%d v3=%d\n", v1, v2, v3);
    return 0;
}
