/* Phase 5.2 Integration: crafting with JSON materials
   - Write temporary items JSON (materials + product)
   - Write temporary materials JSON linking to item ids
   - Write temporary recipe JSON using item ids
   - Load items/materials/recipes, seed inventory, execute craft, assert deltas */
#include "../../src/core/crafting/crafting.h"
#include "../../src/core/crafting/material_registry.h"
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/loot/loot_item_defs.h"
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

static int inv_get_cb(int def_index) { return rogue_inventory_get_count(def_index); }
static int inv_add_cb(int def_index, int qty) { return rogue_inventory_add(def_index, qty); }
static int inv_consume_cb(int def_index, int qty)
{
    return rogue_inventory_consume(def_index, qty);
}

int main(void)
{
    const char* items_path = "tmp_craft_items.json";
    const char* mats_path = "tmp_materials.json";
    const char* recipes_path = "tmp_recipes.json";

    const char* items_json =
        "[\n"
        " {\"id\":\"mat_iron_ore\",\"name\":\"Iron Ore\",\"category\":5,"
        "\"level_req\":1,\"stack_max\":99,\"base_value\":2,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":0,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0},\n"
        " {\"id\":\"prod_iron_ingot\",\"name\":\"Iron Ingot\",\"category\":5,"
        "\"level_req\":1,\"stack_max\":99,\"base_value\":6,\"base_damage_min\":0,\"base_"
        "damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_tx\":1,"
        "\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0}\n"
        "]\n";

    const char* materials_json = "[\n"
                                 " {\"id\":\"iron_ore\",\"item\":\"mat_iron_ore\",\"tier\":0,"
                                 "\"category\":\"ore\",\"base_value\":2}\n"
                                 "]\n";

    const char* recipes_json =
        "[\n"
        " {\"id\":\"smelt_ingot\",\"output\":\"prod_iron_ingot\",\"output_qty\":1,\"inputs\":[{"
        "\"id\":\"mat_iron_ore\",\"qty\":3}],\"time_ms\":1000,\"station\":\"forge\"}\n"
        "]\n";

    if (!write_file(items_path, items_json) || !write_file(mats_path, materials_json) ||
        !write_file(recipes_path, recipes_json))
    {
        fprintf(stderr, "CRAFT_JSON_FAIL write temp files\n");
        return 1;
    }

    rogue_item_defs_reset();
    int added_items = rogue_item_defs_load_from_json(items_path);
    if (added_items < 2)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL items=%d\n", added_items);
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 2;
    }

    rogue_material_registry_reset();
    int mats = rogue_material_registry_load_path(mats_path);
    if (mats <= 0)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL materials=%d\n", mats);
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 3;
    }

    rogue_craft_reset();
    int rec = rogue_craft_load_json(recipes_path);
    if (rec <= 0 || rogue_craft_validate_dependencies() != 0)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL recipes=%d deps=%d\n", rec,
                rogue_craft_validate_dependencies());
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 4;
    }

    const RogueCraftRecipe* r = rogue_craft_find("smelt_ingot");
    if (!r || r->input_count != 1 || r->output_def < 0)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL recipe lookup\n");
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 5;
    }

    int ore_def = rogue_item_def_index("mat_iron_ore");
    int ingot_def = rogue_item_def_index("prod_iron_ingot");
    if (ore_def < 0 || ingot_def < 0)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL def index\n");
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 6;
    }

    rogue_inventory_reset();
    if (rogue_inventory_add(ore_def, 3) < 3)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL seed inventory\n");
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 7;
    }

    int rc = rogue_craft_execute(r, inv_get_cb, inv_consume_cb, inv_add_cb);
    if (rc != 0)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL exec rc=%d\n", rc);
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 8;
    }

    int ore_left = rogue_inventory_get_count(ore_def);
    int ingots = rogue_inventory_get_count(ingot_def);
    if (ore_left != 0 || ingots != 1)
    {
        fprintf(stderr, "CRAFT_JSON_FAIL inv ore=%d ingot=%d\n", ore_left, ingots);
        remove(items_path);
        remove(mats_path);
        remove(recipes_path);
        return 9;
    }

    remove(items_path);
    remove(mats_path);
    remove(recipes_path);
    printf("CRAFT_JSON_OK ingots=%d\n", ingots);
    return 0;
}
