#include "../../src/core/loot/loot_affixes.h"
#include "../../src/util/path_utils.h"
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
    int added = rogue_affixes_load_from_cfg(path);
    if (added < 4)
        return fail("added");
    unsigned int seed = 1337u; /* deterministic */
    int seq[5];
    for (int i = 0; i < 5; i++)
        seq[i] = rogue_affix_roll(ROGUE_AFFIX_PREFIX, 0, &seed);
    rogue_affixes_reset();
    rogue_affixes_load_from_cfg(path);
    seed = 1337u;
    for (int i = 0; i < 5; i++)
    {
        int r = rogue_affix_roll(ROGUE_AFFIX_PREFIX, 0, &seed);
        if (r != seq[i])
            return fail("determinism");
    }
    printf("AFFIX_ROLL_DET_OK first=%d second=%d\n", seq[0], seq[1]);
    return 0;
}
