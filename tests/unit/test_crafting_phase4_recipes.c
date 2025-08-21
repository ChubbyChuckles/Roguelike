/* Crafting & Gathering Phase 4.1–4.2: Recipe parsing + extended fields + forward compat */
#include "core/crafting/crafting.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

/* Minimal inventory callbacks for execute path */
static int inv_counts[512];
static int inv_get(int def_index)
{
    if (def_index < 0 || def_index >= 512)
        return 0;
    return inv_counts[def_index];
}
static int inv_add(int def_index, int qty)
{
    if (def_index < 0 || def_index >= 512)
        return 0;
    inv_counts[def_index] += qty;
    return qty;
}
static int inv_consume(int def_index, int qty)
{
    if (def_index < 0 || def_index >= 512)
        return 0;
    if (inv_counts[def_index] < qty)
        return 0;
    inv_counts[def_index] -= qty;
    return qty;
}

int main(void)
{
    rogue_item_defs_reset();
    const char* dirs[] = {"assets/items", "../assets/items", "../../assets/items",
                          "../../../assets/items"};
    int loaded = 0;
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && loaded <= 0; i++)
    {
        loaded = rogue_item_defs_load_directory(dirs[i]);
    }
    if (loaded <= 0)
    {
        fprintf(stderr, "CRAFT_P4_FAIL items load\n");
        return 10;
    }
    rogue_craft_reset();
    const char* path = "tmp_phase4_recipes.cfg";
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f)
    {
        fprintf(stderr, "CRAFT_P4_FAIL open tmp\n");
        return 11;
    }
    fprintf(f, "dust_to_shard,primal_shard,1,arcane_dust:5,,1500,mystic_altar,20,120,EXTRA_TOKEN_"
               "IGNORED\n");
    fprintf(f, "ore_to_dust,arcane_dust,2,iron_ore:4,,500,forge,5,15\n");
    fclose(f);
    int added = rogue_craft_load_file(path);
    if (added < 2)
    {
        fprintf(stderr, "CRAFT_P4_FAIL parse added=%d\n", added);
        return 12;
    }
    if (rogue_craft_recipe_count() < 2)
    {
        fprintf(stderr, "CRAFT_P4_FAIL count %d\n", rogue_craft_recipe_count());
        return 13;
    }
    const RogueCraftRecipe* r0 = rogue_craft_find("dust_to_shard");
    const RogueCraftRecipe* r1 = rogue_craft_find("ore_to_dust");
    if (!r0 || !r1)
    {
        fprintf(stderr, "CRAFT_P4_FAIL find recipes\n");
        return 14;
    }
    if (r0->time_ms != 1500 || strcmp(r0->station, "mystic_altar") != 0 || r0->skill_req != 20 ||
        r0->exp_reward != 120)
    {
        fprintf(stderr, "CRAFT_P4_FAIL r0 fields time=%d station=%s skill=%d exp=%d\n", r0->time_ms,
                r0->station, r0->skill_req, r0->exp_reward);
        return 15;
    }
    if (r1->time_ms != 500 || strcmp(r1->station, "forge") != 0 || r1->skill_req != 5 ||
        r1->exp_reward != 15)
    {
        fprintf(stderr, "CRAFT_P4_FAIL r1 fields\n");
        return 16;
    }
    if (r0->input_count != 1 || r0->inputs[0].quantity != 5)
    {
        fprintf(stderr, "CRAFT_P4_FAIL r0 inputs\n");
        return 17;
    }
    if (r1->inputs[0].quantity != 4)
    {
        fprintf(stderr, "CRAFT_P4_FAIL r1 inputs\n");
        return 18;
    }
    int ore = r1->inputs[0].def_index;
    int dust = r0->inputs[0].def_index;
    int shard = r0->output_def;
    inv_add(ore, 20); /* craft dust twice then shard */
    if (rogue_craft_execute(r1, inv_get, inv_consume, inv_add) != 0 ||
        rogue_craft_execute(r1, inv_get, inv_consume, inv_add) != 0 ||
        rogue_craft_execute(r1, inv_get, inv_consume, inv_add) != 0)
    {
        fprintf(stderr, "CRAFT_P4_FAIL exec r1 sequences\n");
        return 19;
    }
    if (inv_get(dust) != 6)
    {
        fprintf(stderr, "CRAFT_P4_FAIL dust count=%d\n", inv_get(dust));
        return 20;
    }
    if (rogue_craft_execute(r0, inv_get, inv_consume, inv_add) != 0)
    {
        fprintf(stderr, "CRAFT_P4_FAIL exec r0 shard need=%d have=%d\n", r0->inputs[0].quantity,
                inv_get(r0->inputs[0].def_index));
        return 21;
    }
    if (inv_get(shard) != 1)
    {
        fprintf(stderr, "CRAFT_P4_FAIL shard count=%d\n", inv_get(shard));
        return 22;
    }
    printf("CRAFT_P4_OK recipes=%d time0=%d station0=%s time1=%d station1=%s shard=%d dust=%d\n",
           rogue_craft_recipe_count(), r0->time_ms, r0->station, r1->time_ms, r1->station,
           inv_get(shard), inv_get(dust));
    return 0;
}
