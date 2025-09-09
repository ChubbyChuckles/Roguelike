/* Phase 5.2 Integration: vendor operations with JSON-loaded items
   - Write a temporary items JSON defining a weapon and a consumable
   - Load loot table from a temporary cfg referencing those ids
   - Generate a small vendor inventory and validate counts/prices */
#include "../../src/core/loot/loot_drop_rates.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tables.h"
#include "../../src/core/vendor/vendor.h"
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
    /* Prepare temporary JSON items (ids referenced by loot table below) */
    const char* items_path = "tmp_vendor_items.json";
    const char* items_json =
        "[\n"
        " {\"id\":\"vjson_sword\",\"name\":\"Vendor JSON Sword\",\"category\":2,"
        "\"level_req\":1,\"stack_max\":1,\"base_value\":40,\"base_damage_min\":3,\"base_"
        "damage_max\":6,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":0},\n"
        " {\"id\":\"vjson_potion\",\"name\":\"Vendor JSON Potion\",\"category\":1,"
        "\"level_req\":1,\"stack_max\":10,\"base_value\":8,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":1,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0}\n"
        "]\n";

    if (!write_file(items_path, items_json))
    {
        fprintf(stderr, "VENDOR_JSON_FAIL write items json\n");
        return 1;
    }

    rogue_item_defs_reset();
    int added_items = rogue_item_defs_load_from_json(items_path);
    if (added_items < 2)
    {
        fprintf(stderr, "VENDOR_JSON_FAIL load items added=%d\n", added_items);
        remove(items_path);
        return 2;
    }

    /* Prepare a simple loot table referencing the JSON item ids (newer format) */
    const char* tables_path = "tmp_vendor_loot_tables.cfg";
    const char* tables_cfg =
        /* id,rolls_min,rolls_max, then entries: id,weight,qmin,qmax,rarity_min,rarity_max */
        "VENDOR_JSON_TEST,1,1,vjson_sword,1,1,1,0,4;vjson_potion,1,1,2,0,2\n";
    if (!write_file(tables_path, tables_cfg))
    {
        fprintf(stderr, "VENDOR_JSON_FAIL write tables cfg\n");
        remove(items_path);
        return 3;
    }

    rogue_drop_rates_reset();
    rogue_loot_tables_reset();
    int added_tables = rogue_loot_tables_load_from_cfg(tables_path);
    if (added_tables <= 0)
    {
        fprintf(stderr, "VENDOR_JSON_FAIL load tables=%d\n", added_tables);
        remove(items_path);
        remove(tables_path);
        return 4;
    }
    int t = rogue_loot_table_index("VENDOR_JSON_TEST");
    if (t < 0)
    {
        fprintf(stderr, "VENDOR_JSON_FAIL table index\n");
        remove(items_path);
        remove(tables_path);
        return 5;
    }

    /* Generate vendor inventory from the JSON-backed table */
    rogue_vendor_reset();
    RogueGenerationContext ctx = {
        .enemy_level = 7, .biome_id = 0, .enemy_archetype = 1, .player_luck = 1};
    unsigned int seed = 98765u;
    int gen = rogue_vendor_generate_inventory(t, 4, &ctx, &seed);
    if (gen <= 0 || gen > 4)
    {
        fprintf(stderr, "VENDOR_JSON_FAIL gen=%d\n", gen);
        remove(items_path);
        remove(tables_path);
        return 6;
    }
    if (rogue_vendor_item_count() != gen)
    {
        fprintf(stderr, "VENDOR_JSON_FAIL count=%d gen=%d\n", rogue_vendor_item_count(), gen);
        remove(items_path);
        remove(tables_path);
        return 7;
    }
    for (int i = 0; i < rogue_vendor_item_count(); i++)
    {
        const RogueVendorItem* it = rogue_vendor_get(i);
        if (!it || it->def_index < 0 || it->price <= 0)
        {
            fprintf(stderr, "VENDOR_JSON_FAIL item i=%d def=%d price=%d\n", i,
                    it ? it->def_index : -1, it ? it->price : -1);
            remove(items_path);
            remove(tables_path);
            return 8;
        }
    }

    /* Cleanup temp files */
    remove(items_path);
    remove(tables_path);
    printf("VENDOR_JSON_OK count=%d\n", gen);
    return 0;
}
