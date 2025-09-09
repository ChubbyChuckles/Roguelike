/* Phase 5.2 Integration: loot generation with JSON-loaded items
   - Write a temporary items JSON and a minimal loot table referencing those ids
   - Load affixes (for potential affix rolls)
   - Generate an item via rogue_generate_item and assert fields
*/
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_generation.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_rarity_adv.h"
#include "../../src/core/loot/loot_tables.h"
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
    /* Prepare JSON items */
    const char* items_path = "tmp_gen_items.json";
    const char* items_json =
        "[\n"
        " {\"id\":\"gjson_blade\",\"name\":\"Gen JSON Blade\",\"category\":2,"
        "\"level_req\":1,\"stack_max\":1,\"base_value\":30,\"base_damage_min\":2,\"base_"
        "damage_max\":5,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":0},\n"
        " {\"id\":\"gjson_elixir\",\"name\":\"Gen JSON Elixir\",\"category\":1,"
        "\"level_req\":1,\"stack_max\":5,\"base_value\":5,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":1,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0}\n"
        "]\n";
    if (!write_file(items_path, items_json))
    {
        fprintf(stderr, "GEN_JSON_FAIL write items json\n");
        return 1;
    }

    /* Minimal loot table using the new ids */
    const char* tables_path = "tmp_gen_loot_tables.cfg";
    const char* tables_cfg =
        /* id,rolls_min,rolls_max then entries: id,weight,qmin,qmax,rarity_min,rarity_max */
        "GEN_JSON_TEST,1,1,gjson_blade,1,1,1,0,3;gjson_elixir,1,1,2,0,1\n";
    if (!write_file(tables_path, tables_cfg))
    {
        fprintf(stderr, "GEN_JSON_FAIL write tables cfg\n");
        remove(items_path);
        return 2;
    }

    /* Load affixes and tables/items */
    rogue_drop_rates_reset();
    rogue_affixes_reset();
    /* Try to load affixes from assets path; best-effort, test does not require affixes to exist */
    char apath[256];
    if (rogue_find_asset_path("affixes.cfg", apath, sizeof apath))
        (void) rogue_affixes_load_from_cfg(apath);

    rogue_item_defs_reset();
    int items_added = rogue_item_defs_load_from_json(items_path);
    if (items_added < 2)
    {
        fprintf(stderr, "GEN_JSON_FAIL items=%d\n", items_added);
        remove(items_path);
        remove(tables_path);
        return 3;
    }

    rogue_loot_tables_reset();
    int tables_added = rogue_loot_tables_load_from_cfg(tables_path);
    if (tables_added <= 0)
    {
        fprintf(stderr, "GEN_JSON_FAIL tables=%d\n", tables_added);
        remove(items_path);
        remove(tables_path);
        return 4;
    }
    int tbl = rogue_loot_table_index("GEN_JSON_TEST");
    if (tbl < 0)
    {
        fprintf(stderr, "GEN_JSON_FAIL table index\n");
        remove(items_path);
        remove(tables_path);
        return 5;
    }

    /* Generate */
    RogueGenerationContext ctx = {
        .enemy_level = 12, .biome_id = 0, .enemy_archetype = 1, .player_luck = 3};
    unsigned int seed = 424242u;
    RogueGeneratedItem gi;
    if (rogue_generate_item(tbl, &ctx, &seed, &gi) != 0)
    {
        fprintf(stderr, "GEN_JSON_FAIL gen\n");
        remove(items_path);
        remove(tables_path);
        return 6;
    }
    if (gi.def_index < 0 || gi.rarity < 0)
    {
        fprintf(stderr, "GEN_JSON_FAIL fields di=%d r=%d\n", gi.def_index, gi.rarity);
        remove(items_path);
        remove(tables_path);
        return 7;
    }
    /* Level 12 should floor rarity to at least 1 (12/10 -> 1) */
    if (gi.rarity < 1)
    {
        fprintf(stderr, "GEN_JSON_FAIL rarity_floor=%d\n", gi.rarity);
        remove(items_path);
        remove(tables_path);
        return 8;
    }
    if (gi.inst_index >= 0)
    {
        const RogueItemInstance* inst = rogue_item_instance_at(gi.inst_index);
        if (!inst || inst->def_index != gi.def_index)
        {
            fprintf(stderr, "GEN_JSON_FAIL inst linkage\n");
            remove(items_path);
            remove(tables_path);
            return 9;
        }
    }

    /* cleanup */
    remove(items_path);
    remove(tables_path);
    printf("GEN_JSON_OK def=%d rarity=%d inst=%d\n", gi.def_index, gi.rarity, gi.inst_index);
    return 0;
}
