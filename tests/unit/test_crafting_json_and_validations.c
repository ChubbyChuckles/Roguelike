/* Phase 2.3.3.3, .5, .6, .7: Crafting JSON loader + dependency/balance/skill validation */
#include "core/crafting/crafting.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

static int write_items_json(const char* path)
{
    const char* json =
        "[\n"
        " {\"id\":\"iron_ore\",\"name\":\"Iron "
        "Ore\",\"category\":5,\"level_req\":1,\"stack_max\":99,\"base_value\":8,\"base_damage_"
        "min\":0,\"base_damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_"
        "tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0},\n"
        " {\"id\":\"arcane_dust\",\"name\":\"Arcane "
        "Dust\",\"category\":5,\"level_req\":1,\"stack_max\":99,\"base_value\":9,\"base_damage_"
        "min\":0,\"base_damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_"
        "tx\":1,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0},\n"
        " {\"id\":\"primal_shard\",\"name\":\"Primal "
        "Shard\",\"category\":5,\"level_req\":1,\"stack_max\":99,\"base_value\":50,\"base_damage_"
        "min\":0,\"base_damage_max\":0,\"base_armor\":0,\"sprite_sheet\":\"sheet.png\",\"sprite_"
        "tx\":2,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":1,\"flags\":0}\n"
        "]";
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    return 1;
}

static int write_recipes_json(const char* path)
{
    const char* json = "[\n"
                       " {\"id\":\"ore_to_dust\",\"output\":\"arcane_dust\",\"output_qty\":2,"
                       "\"inputs\":[{\"id\":\"iron_ore\",\"qty\":4}],\"time_ms\":500,\"station\":"
                       "\"forge\",\"skill_req\":5,\"exp_reward\":15},\n"
                       " {\"id\":\"dust_to_shard\",\"output\":\"primal_shard\",\"output_qty\":1,"
                       "\"inputs\":[{\"id\":\"arcane_dust\",\"qty\":5}],\"time_ms\":1500,"
                       "\"station\":\"mystic_altar\",\"skill_req\":20,\"exp_reward\":120}\n"
                       "]";
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "wb") != 0)
        f = NULL;
#else
    f = fopen(path, "wb");
#endif
    if (!f)
        return 0;
    fwrite(json, 1, strlen(json), f);
    fclose(f);
    return 1;
}

int main(void)
{
    rogue_item_defs_reset();
    if (!write_items_json("tmp_items_craft.json"))
    {
        printf("FAIL write items json\n");
        return 1;
    }
    if (rogue_item_defs_load_from_json("tmp_items_craft.json") != 3)
    {
        printf("FAIL load items json\n");
        remove("tmp_items_craft.json");
        return 2;
    }
    rogue_craft_reset();
    if (!write_recipes_json("tmp_recipes.json"))
    {
        printf("FAIL write recipes json\n");
        return 3;
    }
    int added = rogue_craft_load_json("tmp_recipes.json");
    if (added != 2 || rogue_craft_recipe_count() < 2)
    {
        printf("FAIL load recipes json added=%d count=%d\n", added, rogue_craft_recipe_count());
        return 4;
    }
    const RogueCraftRecipe* r0 = rogue_craft_find("ore_to_dust");
    const RogueCraftRecipe* r1 = rogue_craft_find("dust_to_shard");
    if (!r0 || !r1 || r0->time_ms != 500 || strcmp(r0->station, "forge") != 0 ||
        r1->skill_req != 20)
    {
        printf("FAIL recipe fields\n");
        return 5;
    }

    /* Validations */
    int dep = rogue_craft_validate_dependencies();
    if (dep != 0)
    {
        printf("FAIL dependency validation %d\n", dep);
        return 6;
    }
    int bal = rogue_craft_validate_balance(0.1f, 10.0f);
    if (bal != 0)
    {
        printf("FAIL balance validation %d\n", bal);
        return 7;
    }
    int skills = rogue_craft_validate_skill_requirements();
    if (skills != 0)
    {
        printf("FAIL skill validation %d\n", skills);
        return 8;
    }

    remove("tmp_items_craft.json");
    remove("tmp_recipes.json");
    printf("OK crafting JSON + validations (recipes=%d)\n", rogue_craft_recipe_count());
    return 0;
}
