#include "../../src/core/inventory/inventory.h"
#include "../../src/core/inventory/inventory_ui.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/path_utils.h"
#include <stdio.h>
#include <string.h>

static int find_def(const char* id)
{
    int di = rogue_item_def_index(id);
    if (di < 0)
    {
        fprintf(stderr, "NO_DEF %s\n", id);
    }
    return di;
}

int main(void)
{
    char pitems[256];
    if (!rogue_find_asset_path("items/materials.cfg", pitems, sizeof pitems))
    {
        printf("INV13_FAIL find_defs\n");
        return 10;
    }
    for (int i = (int) strlen(pitems) - 1; i >= 0; i--)
    {
        if (pitems[i] == '/' || pitems[i] == '\\')
        {
            pitems[i] = '\0';
            break;
        }
    }
    rogue_item_defs_reset();
    if (rogue_item_defs_load_directory(pitems) <= 0)
    {
        printf("INV13_FAIL load_dir\n");
        return 11;
    }
    rogue_inventory_reset();
    int dust = find_def("arcane_dust");
    int shard = find_def("primal_shard");
    if (dust < 0 || shard < 0)
        return 12;
    rogue_inventory_add(dust, 15);
    rogue_inventory_add(shard, 2);
    int ids[16];
    int counts[16];
    RogueInventoryFilter filter = {0};
    int occ = rogue_inventory_ui_build(ids, counts, 16, ROGUE_INV_SORT_COUNT, &filter);
    if (occ < 2)
    {
        printf("INV13_FAIL build_occ=%d\n", occ);
        return 13;
    }
    if (counts[0] < counts[1])
    {
        printf("INV13_FAIL sort_order counts0=%d counts1=%d\n", counts[0], counts[1]);
        return 14;
    }
    /* Rarity filter test: choose min_rarity high to likely filter out dust (rarity 1) */
    filter.min_rarity = 2;
    int occ2 = rogue_inventory_ui_build(ids, counts, 16, ROGUE_INV_SORT_COUNT, &filter);
    if (occ2 > occ)
    {
        printf("INV13_FAIL filter_occ=%d orig=%d\n", occ2, occ);
        return 15;
    }
    printf("INV13_OK occ=%d occ2=%d\n", occ, occ2);
    return 0;
}
