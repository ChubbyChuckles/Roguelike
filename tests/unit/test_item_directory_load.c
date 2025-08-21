/* Test loading multiple category item definition files via directory loader */
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include <stdio.h>

int main(void)
{
    char dir[256];
    if (!rogue_find_asset_path("items/swords.cfg", dir, sizeof dir))
    {
        fprintf(stderr, "ITEM_DIR_FAIL find swords.cfg\n");
        return 10;
    }
    /* Trim off trailing filename to obtain directory path */
    for (int i = (int) strlen(dir) - 1; i >= 0; i--)
    {
        if (dir[i] == '/' || dir[i] == '\\')
        {
            dir[i] = '\0';
            break;
        }
    }
    rogue_item_defs_reset();
    int total = rogue_item_defs_load_directory(dir);
    if (total <= 0)
    {
        fprintf(stderr, "ITEM_DIR_FAIL total=%d\n", total);
        return 11;
    }
    /* Spot check a few known ids */
    if (rogue_item_def_index("iron_sword") < 0 || rogue_item_def_index("small_heal_potion") < 0 ||
        rogue_item_def_index("leather_armor") < 0)
    {
        fprintf(stderr, "ITEM_DIR_FAIL missing expected ids\n");
        return 12;
    }
    printf("ITEM_DIR_OK total=%d\n", total);
    return 0;
}
