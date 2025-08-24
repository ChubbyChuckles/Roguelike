#include "salvage.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include <stdio.h>

/* Simple salvage rule: materials per rarity = 1,2,4,8,16 scaled by base_value bracket. Uses
 * 'arcane_dust' for rarity <3, 'primal_shard' for rarity >=3 if present. */

static int find_material(const char* id) { return rogue_item_def_index(id); }

int rogue_salvage_item(int item_def_index, int rarity, int (*add_material_cb)(int, int))
{
    const RogueItemDef* d = rogue_item_def_at(item_def_index);
    if (!d || !add_material_cb)
        return 0;
    int mult[5] = {1, 2, 4, 8, 16};
    if (rarity < 0)
        rarity = 0;
    if (rarity > 4)
        rarity = 4;
    int base = mult[rarity];
    /* Scale by value bracket (<=50 -> x1, <=150 -> x2, >150 -> x3) */
    int scale = (d->base_value > 150) ? 3 : (d->base_value > 50 ? 2 : 1);
    int qty = base * scale;
    const char* mat_id = (rarity >= 3) ? "primal_shard" : "arcane_dust";
    int mat_def = find_material(mat_id);
    if (mat_def < 0)
    {
        fprintf(stderr, "salvage: missing material def %s\n", mat_id);
        return 0;
    }
    if (qty < 1)
        qty = 1;
    add_material_cb(mat_def, qty);
    return qty;
}

int rogue_salvage_item_instance(int inst_index, int (*add_material_cb)(int def_index, int qty))
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    if (!it)
        return 0;
    const RogueItemDef* d = rogue_item_def_at(it->def_index);
    if (!d)
        return 0;
    int base_qty = rogue_salvage_item(it->def_index, d->rarity,
                                      add_material_cb); /* this already adds materials */
    if (base_qty <= 0)
        return 0; /* salvage_item already applied; we need to adjust to durability scaling: so
                     rollback not feasible -> apply delta */
    if (it->durability_max > 0)
    {
        float pct = (it->durability_max > 0)
                        ? (float) it->durability_cur / (float) it->durability_max
                        : 0.0f;
        if (pct < 0)
            pct = 0;
        if (pct > 1)
            pct = 1;
        float factor = 0.4f + 0.6f * pct; /* 40% floor */
        int target = (int) ((float) base_qty * factor + 0.5f);
        if (target < 1)
            target = 1;
        if (target == base_qty)
            return base_qty;
        int diff = target - base_qty;
        if (diff > 0)
        { /* grant additional materials to reach target */
            const char* mat_id = (d->rarity >= 3) ? "primal_shard" : "arcane_dust";
            int mat_def = rogue_item_def_index(mat_id);
            if (mat_def >= 0)
                add_material_cb(mat_def, diff);
        }
        else if (diff < 0)
        { /* cannot retract already added materials; log for awareness */
            fprintf(stderr,
                    "salvage_item_instance: negative diff not applied (base=%d target=%d)\n",
                    base_qty, target);
        }
        return (diff > 0) ? target : base_qty; /* best approximation of delivered quantity */
    }
    return base_qty;
}
