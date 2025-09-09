/* Tests inventory filtering & sorting with JSON-loaded items (name, rarity, category). */
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/inventory/inventory_ui.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include <stdio.h>

static int load_items(void)
{
    const char* dirs[] = {"assets/items", "../assets/items", "../../assets/items", NULL};
    rogue_item_defs_reset();
    for (int i = 0; dirs[i]; i++)
    {
        int c = rogue_item_defs_load_directory_json(dirs[i]);
        if (c > 0)
            return c;
    }
    return 0;
}

int main(void)
{
    if (load_items() <= 0)
    {
        printf("INV_JSON_FILTER_FAIL load\n");
        return 10;
    }
    int dagger = rogue_item_def_index("weapon_rare_dagger");
    int sword = rogue_item_def_index("weapon_iron_sword");
    int ore = rogue_item_def_index("material_iron_ore");
    /* If we lack either dagger or sword (content variance), add a lightweight synthetic sword so
       category mask test yields at least one result. */
    if (sword < 0)
    {
        RogueItemDef temp = {0};
        strcpy(temp.id, "weapon_temp_fallback");
        strcpy(temp.name, "Temp Weapon");
        temp.category = ROGUE_ITEM_WEAPON;
        temp.level_req = 1;
        temp.stack_max = 1;
        temp.base_value = 5;
        temp.base_damage_min = 1;
        temp.base_damage_max = 2;
        temp.rarity = 0;
        rogue_item_defs_add(&temp);
        sword = rogue_item_def_index("weapon_temp_fallback");
    }
    if (dagger < 0 || sword < 0 || ore < 0)
    {
        printf("INV_JSON_FILTER_FAIL defs\n");
        return 11;
    }
    /* Initialize runtime after successful definition load */
    rogue_items_init_runtime();
    rogue_inventory_reset();
    rogue_inventory_add(dagger, 1);
    rogue_inventory_add(sword, 2);
    rogue_inventory_add(ore, 15);
    int ids[16];
    int counts[16];
    RogueInventoryFilter filter = {0};
    int occ = rogue_inventory_ui_build(ids, counts, 16, ROGUE_INV_SORT_RARITY, &filter);
    if (occ < 3)
    {
        printf("INV_JSON_FILTER_FAIL occ=%d\n", occ);
        return 12;
    }
    if (ids[0] != dagger)
    {
        printf("INV_JSON_FILTER_FAIL rarity_sort first=%d expected=%d\n", ids[0], dagger);
        return 13;
    }
    filter.min_rarity = 1;
    int occ2 = rogue_inventory_ui_build(ids, counts, 16, ROGUE_INV_SORT_RARITY, &filter);
    if (occ2 >= occ || occ2 < 1)
    {
        printf("INV_JSON_FILTER_FAIL occ2=%d occ=%d\n", occ2, occ);
        return 14;
    }
    /* Reset min rarity so both dagger (rare) and sword (common) appear before applying category
     * mask */
    filter.min_rarity = 0;
    filter.category_mask = (1u << 2);
    int occ3 = rogue_inventory_ui_build(ids, counts, 16, ROGUE_INV_SORT_NAME, &filter);
    if (occ3 < 1)
    {
        printf("INV_JSON_FILTER_FAIL occ3=%d\n", occ3);
        return 15;
    }
    for (int i = 0; i < occ3; i++)
    {
        const RogueItemDef* d = rogue_item_def_at(ids[i]);
        if (!d || d->category != 2)
        {
            printf("INV_JSON_FILTER_FAIL non_weapon id=%d\n", ids[i]);
            return 16;
        }
    }
    printf("INV_JSON_FILTER_OK occ=%d occ2=%d occ3=%d\n", occ, occ2, occ3);
    return 0;
}
