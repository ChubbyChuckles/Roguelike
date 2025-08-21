/* Crafting Phase 4.3–4.6: Station registry, queue model, cancel/refund, determinism */
#include "core/crafting/crafting.h"
#include "core/crafting_queue.h"
#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

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
    /* Reset to ensure fresh load count even if other tests executed first */
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
        fprintf(stderr, "CRAFT_P4Q_FAIL items load\n");
        return 10;
    }
    rogue_craft_reset();
    rogue_craft_queue_reset();
    /* Build two recipes (ore->dust, dust->shard) with different stations & times */
    const char* path = "tmp_phase4_queue.cfg";
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "wb");
#else
    f = fopen(path, "wb");
#endif
    if (!f)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL open tmp\n");
        return 11;
    }
    fprintf(f, "ore_to_dust,arcane_dust,2,iron_ore:4,,400,forge,0,0\n");
    fprintf(f, "dust_to_shard,primal_shard,1,arcane_dust:5,,800,mystic_altar,0,0\n");
    fclose(f);
    if (rogue_craft_load_file(path) < 2)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL load recipes\n");
        return 12;
    }
    int ore_def = rogue_item_def_index("iron_ore");
    int dust_def = rogue_item_def_index("arcane_dust");
    int shard_def = rogue_item_def_index("primal_shard");
    if (ore_def < 0 || dust_def < 0 || shard_def < 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL def lookup\n");
        return 13;
    }
    inv_add(ore_def, 40); /* enough for multiple cycles */
    const RogueCraftRecipe* r_dust = rogue_craft_find("ore_to_dust");
    const RogueCraftRecipe* r_shard = rogue_craft_find("dust_to_shard");
    if (!r_dust || !r_shard)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL find\n");
        return 14;
    }
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
        fprintf(stderr, "CRAFT_P4Q_FAIL index\n");
        return 15;
    }
    /* Enqueue three dust jobs (forge capacity 2 -> one waiting) */
    int j1 = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
    int j2 = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
    int j3 = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
    if (j1 < 0 || j2 < 0 || j3 < 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL enqueue dust\n");
        return 16;
    }
    int active_initial = rogue_craft_queue_active_count(rogue_craft_station_id("forge"));
    if (active_initial != 2)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL active_initial=%d\n", active_initial);
        return 17;
    }
    /* Advance 400ms -> first two complete, third activates */
    rogue_craft_queue_update(400, inv_add);
    if (inv_get(dust_def) != 4)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL dust_out=%d\n", inv_get(dust_def));
        return 18;
    }
    int active_after = rogue_craft_queue_active_count(rogue_craft_station_id("forge"));
    if (active_after != 1)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL active_after=%d\n", active_after);
        return 19;
    }
    /* Cancel the third (active) job -> partial refund (inputs 4 -> refund 2) */
    if (rogue_craft_queue_cancel(j3, r_dust, inv_add) != 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL cancel_active\n");
        return 20;
    }
    if (inv_get(ore_def) < 40 - (4 + 4 + 4) + 2)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL cancel_refund ore=%d\n", inv_get(ore_def));
        return 21;
    }
    /* Convert dust->shard (need 5 dust, currently 4 -> enqueue another dust job) */
    int j4 = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
    if (j4 < 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL enqueue j4\n");
        return 22;
    }
    rogue_craft_queue_update(400, inv_add); /* completes j4 */
    if (inv_get(dust_def) != 6)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL dust_after_j4=%d\n", inv_get(dust_def));
        return 23;
    }
    /* Produce two more dust crafts (forge capacity lets them run in parallel) so we have 10 dust
     * total enabling two shard enqueues */
    int j5 = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
    int j6 = rogue_craft_queue_enqueue(r_dust, idx_dust, 0, inv_get, inv_consume);
    if (j5 < 0 || j6 < 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL enqueue extra dust\n");
        return 231;
    }
    rogue_craft_queue_update(400, inv_add);
    if (inv_get(dust_def) != 10)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL dust_after_extra=%d\n", inv_get(dust_def));
        return 232;
    }
    /* Enqueue first shard job (starts immediately given altar cap=1 consuming 5 dust, leaving 5 for
     * waiting job) */
    int jS = rogue_craft_queue_enqueue(r_shard, idx_shard, 0, inv_get, inv_consume);
    if (jS < 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL enqueue shard\n");
        return 24;
    }
    int dust_before_second = inv_get(dust_def);
    if (dust_before_second < 5)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL pre_second_shard_dust=%d\n", dust_before_second);
        return 241;
    }
    /* Enqueue second shard job (should wait) then cancel it for full refund */
    int jS2 = rogue_craft_queue_enqueue(r_shard, idx_shard, 0, inv_get, inv_consume);
    if (jS2 < 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL enqueue shard2 code=%d\n", jS2);
        return 25;
    }
    if (rogue_craft_queue_cancel(jS2, r_shard, inv_add) != 0)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL cancel_waiting\n");
        return 26;
    }
    /* Advance shard completion */
    rogue_craft_queue_update(800, inv_add);
    if (inv_get(shard_def) != 1)
    {
        fprintf(stderr, "CRAFT_P4Q_FAIL shard_out=%d\n", inv_get(shard_def));
        return 27;
    }
    printf("CRAFT_P4Q_OK jobs=%d dust=%d shard=%d ore=%d\n", rogue_craft_queue_job_count(),
           inv_get(dust_def), inv_get(shard_def), inv_get(ore_def));
    return 0;
}
