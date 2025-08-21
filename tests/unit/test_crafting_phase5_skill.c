/* Phase 5: Crafting Skill & Proficiency Progression Tests */
#include "../../src/core/crafting/crafting_queue.h"
#include "../../src/core/crafting/crafting_skill.h"
#include "core/crafting/crafting.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

static int inv_counts[512];
static int inv_get(int d)
{
    if (d < 0 || d >= 512)
        return 0;
    return inv_counts[d];
}
static int inv_add(int d, int q)
{
    if (d < 0 || d >= 512)
        return 0;
    inv_counts[d] += q;
    return q;
}
static int inv_consume(int d, int q)
{
    if (d < 0 || d >= 512)
        return 0;
    if (inv_counts[d] < q)
        return 0;
    inv_counts[d] -= q;
    return q;
}

int main(void)
{
    rogue_item_defs_reset();
    const char* dirs[] = {"assets/items", "../assets/items", "../../assets/items"};
    int loaded = 0;
    for (size_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && loaded <= 0; i++)
    {
        loaded = rogue_item_defs_load_directory(dirs[i]);
    }
    if (loaded <= 0)
    {
        fprintf(stderr, "CRAFT_P5_FAIL load items\n");
        return 10;
    }
    rogue_craft_reset();
    rogue_craft_queue_reset();
    rogue_craft_skill_reset();
    /* Simple recipe file with increasing exp to test level progression and perks */
    const char* path = "tmp_phase5_skill.cfg";
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f)
    {
        fprintf(stderr, "CRAFT_P5_FAIL open tmp\n");
        return 11;
    }
    /* smithing dust recipe (fast) and shard (slower) with exp rewards */
    /* Increase exp reward so that 10 crafts reach at least level 5 (cost perk threshold) */
    fprintf(f, "ore_to_dust5,arcane_dust,1,iron_ore:2,,200,forge,0,100\n");
    fprintf(f, "dust_to_shard5,primal_shard,1,arcane_dust:5,,800,mystic_altar,0,120\n");
    fclose(f);
    if (rogue_craft_load_file(path) < 2)
    {
        fprintf(stderr, "CRAFT_P5_FAIL load recipes\n");
        return 12;
    }
    int ore = rogue_item_def_index("iron_ore");
    int dust = rogue_item_def_index("arcane_dust");
    int shard = rogue_item_def_index("primal_shard");
    if (ore < 0 || dust < 0 || shard < 0)
    {
        fprintf(stderr, "CRAFT_P5_FAIL def lookup\n");
        return 13;
    }
    inv_add(ore, 200);
    const RogueCraftRecipe* r_dust = rogue_craft_find("ore_to_dust5");
    const RogueCraftRecipe* r_shard = rogue_craft_find("dust_to_shard5");
    int idx_dust = -1, idx_shard = -1;
    for (int i = 0; i < rogue_craft_recipe_count(); i++)
    {
        const RogueCraftRecipe* r = rogue_craft_recipe_at(i);
        if (r == r_dust)
            idx_dust = i;
        if (r == r_shard)
            idx_shard = i;
    }
    if (idx_dust < 0 || idx_shard < 0)
    {
        fprintf(stderr, "CRAFT_P5_FAIL index\n");
        return 14;
    }
    /* Enqueue a batch of dust crafts to gain smithing XP */
    for (int i = 0; i < 10; i++)
    {
        if (rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume) < 0)
        {
            fprintf(stderr, "CRAFT_P5_FAIL enqueue dust i=%d\n", i);
            return 15;
        }
    }
    /* process in steps of 200ms until all delivered */
    for (int t = 0; t < 10; t++)
    {
        rogue_craft_queue_update(200, inv_add);
    }
    int lvl_smith = rogue_craft_skill_level(ROGUE_CRAFT_DISC_SMITHING);
    if (lvl_smith <= 0)
    {
        fprintf(stderr, "CRAFT_P5_FAIL smith_level=%d\n", lvl_smith);
        return 16;
    }
    int cost_pct = rogue_craft_perk_material_cost_pct(ROGUE_CRAFT_DISC_SMITHING);
    if (cost_pct >= 100)
    {
        fprintf(stderr, "CRAFT_P5_FAIL perk_cost_pct=%d\n", cost_pct);
        return 17;
    }
    /* produce arcane dust to craft shard and test enchanting XP (altar maps to enchanting) */
    while (inv_get(dust) < 5)
    {
        int id = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
        if (id < 0)
        {
            fprintf(stderr, "CRAFT_P5_FAIL more dust enqueue\n");
            return 18;
        }
        rogue_craft_queue_update(200, inv_add);
    }
    int pre_enchant_xp = rogue_craft_skill_xp(ROGUE_CRAFT_DISC_ENCHANTING);
    int shard_job = rogue_craft_queue_enqueue(r_shard, idx_shard, 0, inv_get, inv_consume);
    if (shard_job < 0)
    {
        fprintf(stderr, "CRAFT_P5_FAIL enqueue shard\n");
        return 19;
    }
    for (int t = 0; t < 5; t++)
    {
        rogue_craft_queue_update(200, inv_add);
    }
    if (inv_get(shard) <= 0)
    {
        fprintf(stderr, "CRAFT_P5_FAIL shard_out=%d\n", inv_get(shard));
        return 20;
    }
    if (rogue_craft_skill_xp(ROGUE_CRAFT_DISC_ENCHANTING) <= pre_enchant_xp)
    {
        fprintf(stderr, "CRAFT_P5_FAIL enchant_xp_no_gain\n");
        return 21;
    }
    printf("CRAFT_P5_OK smith_lvl=%d cost_pct=%d dust=%d shard=%d enchant_xp=%d\n", lvl_smith,
           cost_pct, inv_get(dust), inv_get(shard),
           rogue_craft_skill_xp(ROGUE_CRAFT_DISC_ENCHANTING));
    return 0;
}
