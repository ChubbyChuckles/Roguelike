/* Vendor System Phase 0 Tests (0.1-0.5) */
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/path_utils.h"
#include "../../src/core/vendor/econ_materials.h"
#include "../../src/core/vendor/econ_value.h"
#include "../../src/core/vendor/vendor.h"
#include <stdio.h>
#include <string.h>

static int ensure_items_loaded(void)
{
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        fprintf(stderr, "VENDOR_P0_FAIL find test_items.cfg\n");
        return 0;
    }
    rogue_item_defs_reset();
    int items = rogue_item_defs_load_from_cfg(pitems);
    if (items <= 0)
    {
        fprintf(stderr, "VENDOR_P0_FAIL load items=%d\n", items);
        return 0;
    }
    /* Load extended materials (arcane_dust, primal_shard) */
    char pmats[256];
    if (rogue_find_asset_path("items/materials.cfg", pmats, sizeof pmats))
    {
        int mats = rogue_item_defs_load_from_cfg(pmats);
        if (mats <= 0)
        {
            fprintf(stderr, "VENDOR_P0_WARN load materials=%d (continuing)\n", mats);
        }
    }
    else
    {
        fprintf(stderr, "VENDOR_P0_WARN missing materials.cfg path (continuing)\n");
    }
    return 1;
}

int main(void)
{
    if (!ensure_items_loaded())
        return 10;
    int mat_count = rogue_econ_material_catalog_build();
    if (mat_count <= 0)
    {
        fprintf(stderr, "VENDOR_P0_FAIL catalog count=%d\n", mat_count);
        return 11;
    }
    int found_arcane = 0, found_primal = 0;
    for (int i = 0; i < mat_count; i++)
    {
        const RogueMaterialEntry* e = rogue_econ_material_catalog_get(i);
        if (!e)
            return 12;
        if (e->base_value <= 0)
        {
            fprintf(stderr, "VENDOR_P0_FAIL mat base val<=0 idx=%d\n", i);
            return 13;
        }
        const RogueItemDef* d = rogue_item_def_at(e->def_index);
        if (d)
        {
            if (strcmp(d->id, "arcane_dust") == 0)
                found_arcane = 1;
            if (strcmp(d->id, "primal_shard") == 0)
                found_primal = 1;
        }
    }
    if (!found_arcane || !found_primal)
    {
        fprintf(stderr, "VENDOR_P0_FAIL missing mats arc=%d pri=%d\n", found_arcane, found_primal);
        return 14;
    }
    int weapon_def = -1;
    int total = rogue_item_defs_count();
    for (int i = 0; i < total; i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        if (weapon_def < 0 && d->category == ROGUE_ITEM_WEAPON)
            weapon_def = i;
    }
    if (weapon_def < 0)
        weapon_def = 0;
    int last = 0;
    for (int r = 0; r < 5; r++)
    {
        int v = rogue_econ_item_value(weapon_def, r, 0, 1.0f);
        if (v < last)
        {
            fprintf(stderr, "VENDOR_P0_FAIL rarity monotonic r=%d v=%d last=%d\n", r, v, last);
            return 20;
        }
        last = v;
    }
    int base = rogue_econ_item_value(weapon_def, 2, 0, 1.0f);
    int mid = rogue_econ_item_value(weapon_def, 2, 10, 1.0f);
    int high = rogue_econ_item_value(weapon_def, 2, 30, 1.0f);
    if (!(base <= mid && mid <= high))
    {
        fprintf(stderr, "VENDOR_P0_FAIL affix monotonic base=%d mid=%d high=%d\n", base, mid, high);
        return 21;
    }
    int full = rogue_econ_item_value(weapon_def, 2, 10, 1.0f);
    int broken = rogue_econ_item_value(weapon_def, 2, 10, 0.0f);
    if (broken < 1)
    {
        fprintf(stderr, "VENDOR_P0_FAIL broken<1 %d\n", broken);
        return 22;
    }
    if (full <= broken)
    {
        fprintf(stderr, "VENDOR_P0_FAIL durability ordering full=%d broken=%d\n", full, broken);
        return 23;
    }
    printf("VENDOR_PHASE0_OK mats=%d val_full=%d val_broken=%d\n", mat_count, full, broken);
    return 0;
}
