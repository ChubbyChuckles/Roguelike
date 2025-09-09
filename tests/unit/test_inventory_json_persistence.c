/* Phase 5.2 Integration: inventory entries persistence with JSON-loaded items
   - Write temporary items JSON (two materials)
   - Add inventory counts, save slot, reset, load slot, verify counts restored */
#include "../../src/core/inventory/inventory_entries.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/persistence/save_internal.h"
#include "../../src/core/persistence/save_manager.h"
#include <stdio.h>
#include <string.h>

static int write_file(const char* path, const char* contents)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(contents, 1, strlen(contents), f);
    fclose(f);
    return 1;
}

int main(void)
{
    const char* items_path = "tmp_inv_items.json";
    const char* items_json =
        "[\n"
        " {\"id\":\"invjson_herb\",\"name\":\"Herb\",\"category\":0,"
        "\"level_req\":1,\"stack_max\":999,\"base_value\":1,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0},\n"
        " {\"id\":\"invjson_potion\",\"name\":\"Potion\",\"category\":0,"
        "\"level_req\":1,\"stack_max\":999,\"base_value\":5,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0}\n"
        "]\n";
    if (!write_file(items_path, items_json))
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL write items\n");
        return 1;
    }

    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_json(items_path);
    if (added < 2)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL items=%d\n", added);
        remove(items_path);
        return 2;
    }

    /* Init inventory entries */
    rogue_inventory_entries_init();

    int herb = rogue_item_def_index("invjson_herb");
    int potion = rogue_item_def_index("invjson_potion");
    if (herb < 0 || potion < 0)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL def indices h=%d p=%d\n", herb, potion);
        remove(items_path);
        return 3;
    }

    /* Add counts via entries API and verify */
    if (rogue_inventory_register_pickup(herb, 7ull) != 0 ||
        rogue_inventory_register_pickup(potion, 3ull) != 0)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL register_pickup\n");
        remove(items_path);
        return 4;
    }
    if (rogue_inventory_quantity(herb) != 7ull || rogue_inventory_quantity(potion) != 3ull)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL pre-save qty h=%llu p=%llu\n",
                (unsigned long long) rogue_inventory_quantity(herb),
                (unsigned long long) rogue_inventory_quantity(potion));
        remove(items_path);
        return 4;
    }

    /* Configure save manager and sandbox paths */
    rogue_save_manager_init();
    rogue_register_core_save_components();
    rogue_save_paths_set_prefix_tests();

    if (rogue_save_manager_save_slot(0) != 0)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL save\n");
        remove(items_path);
        return 5;
    }

    /* Reset entries state, then verify cleared */
    rogue_inventory_entries_init();
    if (rogue_inventory_quantity(herb) != 0ull || rogue_inventory_quantity(potion) != 0ull)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL reset didn't clear qty h=%llu p=%llu\n",
                (unsigned long long) rogue_inventory_quantity(herb),
                (unsigned long long) rogue_inventory_quantity(potion));
        remove(items_path);
        return 6;
    }

    if (rogue_save_manager_load_slot(0) != 0)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL load\n");
        remove(items_path);
        return 7;
    }

    if (rogue_inventory_quantity(herb) != 7ull || rogue_inventory_quantity(potion) != 3ull)
    {
        fprintf(stderr, "INV_JSON_PERSIST_FAIL post-load qty h=%llu p=%llu\n",
                (unsigned long long) rogue_inventory_quantity(herb),
                (unsigned long long) rogue_inventory_quantity(potion));
        remove(items_path);
        return 8;
    }

    /* Cleanup */
    (void) rogue_save_manager_delete_slot(0);
    remove(items_path);
    printf("INV_JSON_PERSIST_OK herb=%llu potion=%llu\n",
           (unsigned long long) rogue_inventory_quantity(herb),
           (unsigned long long) rogue_inventory_quantity(potion));
    return 0;
}
