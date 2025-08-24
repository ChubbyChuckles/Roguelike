/* Phase 2.3.3.1 & 2.3.3.2: Materials + Gathering JSON loader smoke test */
#include "../../src/core/crafting/gathering.h"
#include "../../src/core/crafting/material_registry.h"
#include "../../src/core/loot/loot_item_defs.h"
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
        "tx\":1,\"sprite_ty\":0,\"sprite_tw\":1,\"sprite_th\":1,\"rarity\":0,\"flags\":0}\n"
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

static int write_materials_json(const char* path)
{
    const char* json = "[\n"
                       " {\"id\":\"iron_ore_mat\",\"item\":\"iron_ore\",\"tier\":0,\"category\":"
                       "\"ore\",\"base_value\":8},\n"
                       " {\"id\":\"arcane_dust_mat\",\"item\":\"arcane_dust\",\"tier\":1,"
                       "\"category\":\"essence\",\"base_value\":9}\n"
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

static int write_gather_nodes_json(const char* path)
{
    const char* json =
        "[\n"
        " {\"id\":\"iron_vein\",\"materials\":[{\"id\":\"iron_ore_mat\",\"weight\":3},{\"id\":"
        "\"arcane_dust_mat\",\"weight\":1}],\"min_roll\":1,\"max_roll\":3,\"respawn_ms\":60000,"
        "\"tool_req_tier\":0,\"biome_tags\":\"mountain\",\"spawn_chance_pct\":100,\"rare_proc_"
        "chance_pct\":10,\"rare_bonus_multiplier\":2.0}\n"
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
    /* Items to back materials */
    rogue_item_defs_reset();
    if (!write_items_json("tmp_items_mg.json"))
    {
        printf("FAIL write items json\n");
        return 1;
    }
    if (rogue_item_defs_load_from_json("tmp_items_mg.json") != 2)
    {
        printf("FAIL load items json\n");
        remove("tmp_items_mg.json");
        return 2;
    }

    /* Materials JSON */
    rogue_material_registry_reset();
    if (!write_materials_json("tmp_materials.json"))
    {
        printf("FAIL write materials json\n");
        return 3;
    }
    int m = rogue_material_registry_load_path("tmp_materials.json");
    if (m != 2)
    {
        printf("FAIL load materials json count=%d\n", m);
        remove("tmp_materials.json");
        remove("tmp_items_mg.json");
        return 4;
    }
    int ore0 = rogue_material_find_by_category_and_tier(ROGUE_MAT_ORE, 0);
    int ess1 = rogue_material_find_by_category_and_tier(ROGUE_MAT_ESSENCE, 1);
    if (ore0 < 0 || ess1 < 0)
    {
        printf("FAIL material category/tier lookup ore0=%d ess1=%d\n", ore0, ess1);
        return 5;
    }
    int next = rogue_material_next_tier_index(ore0);
    if (next != -1)
    {
        printf("FAIL next tier for ore should be -1 got %d\n", next);
        return 6;
    }

    /* Gathering JSON */
    rogue_gather_defs_reset();
    if (!write_gather_nodes_json("tmp_gather.json"))
    {
        printf("FAIL write gather json\n");
        return 7;
    }
    int g = rogue_gather_defs_load_path("tmp_gather.json");
    if (g != 1 || rogue_gather_def_count() != 1)
    {
        printf("FAIL load gather defs g=%d count=%d\n", g, rogue_gather_def_count());
        return 8;
    }
    const RogueGatherNodeDef* def = rogue_gather_def_at(0);
    if (!def || strcmp(def->id, "iron_vein") != 0 || def->material_count < 1)
    {
        printf("FAIL gather def contents\n");
        return 9;
    }
    /* Spawn and harvest deterministically */
    rogue_gather_set_player_tool_tier(0);
    int spawned = rogue_gather_spawn_chunk(12345u, 7);
    if (spawned <= 0 || rogue_gather_node_count() <= 0)
    {
        printf("FAIL spawn nodes\n");
        return 10;
    }
    unsigned int rng = 42u;
    int mat_def = -1, qty = 0;
    if (rogue_gather_harvest(0, &rng, &mat_def, &qty) != 0 || qty < def->min_roll)
    {
        printf("FAIL harvest mat=%d qty=%d\n", mat_def, qty);
        return 11;
    }

    /* Cleanup temp files */
    remove("tmp_items_mg.json");
    remove("tmp_materials.json");
    remove("tmp_gather.json");
    printf("OK materials+gathering JSON (m=%d, nodes=%d)\n", m, rogue_gather_node_count());
    return 0;
}
