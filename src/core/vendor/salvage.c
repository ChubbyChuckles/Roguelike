#include "salvage.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include <stdio.h>

/* Simple salvage rule: materials per rarity = 1,2,4,8,16 scaled by base_value bracket. Uses
 * 'arcane_dust' for rarity <3, 'primal_shard' for rarity >=3 if present. */

static int find_material(const char* id) { return rogue_item_def_index(id); }

/* Internal: compute base salvage material and quantity (pre-durability). Returns material def
 * index in *out_mat_def and quantity in *out_qty. Returns 0 on success, -1 on failure. */
static int salvage_compute_base(int item_def_index, int rarity, int* out_mat_def, int* out_qty)
{
    const RogueItemDef* d = rogue_item_def_at(item_def_index);
    int mult[5] = {1, 2, 4, 8, 16};
    int base, scale, qty, mat_def;
    const char* mat_id;
    if (!d || !out_mat_def || !out_qty)
        return -1;
    if (rarity < 0)
        rarity = 0;
    if (rarity > 4)
        rarity = 4;
    base = mult[rarity];
    /* Scale by value bracket (<=50 -> x1, <=150 -> x2, >150 -> x3) */
    scale = (d->base_value > 150) ? 3 : (d->base_value > 50 ? 2 : 1);
    qty = base * scale;
    mat_id = (rarity >= 3) ? "primal_shard" : "arcane_dust";
    mat_def = find_material(mat_id);
    if (mat_def < 0)
    {
        fprintf(stderr, "salvage: missing material def %s\n", mat_id);
        return -1;
    }
    if (qty < 1)
        qty = 1;
    *out_mat_def = mat_def;
    *out_qty = qty;
    return 0;
}

int rogue_salvage_item(int item_def_index, int rarity, int (*add_material_cb)(int, int))
{
    int mat_def, qty;
    if (!add_material_cb)
        return 0;
    if (salvage_compute_base(item_def_index, rarity, &mat_def, &qty) != 0)
        return 0;
    add_material_cb(mat_def, qty);
    return qty;
}

int rogue_salvage_item_instance(int inst_index, int (*add_material_cb)(int def_index, int qty))
{
    const RogueItemInstance* it = rogue_item_instance_at(inst_index);
    const RogueItemDef* d;
    int mat_def, base_qty, grant_qty;
    float pct, factor;
    if (!it || !add_material_cb)
        return 0;
    d = rogue_item_def_at(it->def_index);
    if (!d)
        return 0;
    if (salvage_compute_base(it->def_index, d->rarity, &mat_def, &base_qty) != 0)
        return 0;
    if (it->durability_max > 0)
    {
        pct = (float) it->durability_cur / (float) it->durability_max;
        if (pct < 0)
            pct = 0;
        if (pct > 1)
            pct = 1;
        /* 40% floor when fully broken; linearly scales up to 100% at full durability */
        factor = 0.4f + 0.6f * pct;
        grant_qty = (int) (base_qty * factor + 0.5f);
        if (grant_qty < 1)
            grant_qty = 1;
    }
    else
    {
        grant_qty = base_qty;
    }
    add_material_cb(mat_def, grant_qty);
    return grant_qty;
}
