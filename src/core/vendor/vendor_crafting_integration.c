#include "vendor_crafting_integration.h"
#include "../crafting/crafting.h"
#include "../crafting/crafting_skill.h"
#include "../crafting/material_refine.h"
#include "../crafting/material_registry.h"
#include <string.h>

/* Simple deficit tracking array (per material def index) */
#ifndef ROGUE_VENDOR_SCARCITY_CAP
#define ROGUE_VENDOR_SCARCITY_CAP 256
#endif
static int g_vendor_scarcity[ROGUE_VENDOR_SCARCITY_CAP];

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return v;
}

int rogue_vendor_purchase_recipe_unlock(int recipe_index, int gold_cost,
                                        int (*spend_gold_cb)(int gold),
                                        void (*on_unlocked_cb)(int recipe_index))
{
    if (recipe_index < 0 || recipe_index >= rogue_craft_recipe_count())
        return -1;
    if (!spend_gold_cb)
        return -2;
    if (!rogue_craft_recipe_is_discovered(recipe_index))
    {
        if (spend_gold_cb(gold_cost) != 0)
            return -3; /* insufficient gold */
        rogue_craft_recipe_mark_discovered(recipe_index);
        if (on_unlocked_cb)
            on_unlocked_cb(recipe_index);
    }
    return 0;
}

int rogue_vendor_batch_refine(int material_def_index, int from_quality, int to_quality,
                              int batch_count, int consume_count, int gold_fee_pct, int base_value,
                              int (*spend_gold_cb)(int gold))
{
    if (material_def_index < 0 || from_quality < 0 || to_quality <= from_quality ||
        batch_count <= 0 || consume_count <= 0)
        return -1;
    if (!spend_gold_cb)
        return -2;
    if (gold_fee_pct < 0)
        gold_fee_pct = 0;
    if (gold_fee_pct > 90)
        gold_fee_pct = 90;
    const RogueMaterialDef* md = rogue_material_get(material_def_index);
    if (md && md->base_value > 0)
        base_value = md->base_value;
    long long total_units = (long long) batch_count * consume_count;
    long long fee = ((long long) base_value * total_units * gold_fee_pct) / 100;
    if (fee > 2000000000LL)
        fee = 2000000000LL;
    if (spend_gold_cb((int) fee) != 0)
        return -3; /* insufficient gold */
    int promoted_total = 0;
    for (int i = 0; i < batch_count; i++)
    {
        int rc = rogue_material_refine(material_def_index, from_quality, to_quality, consume_count,
                                       NULL, NULL, NULL);
        if (rc > 0)
            promoted_total += rc; /* rc returns promoted count */
    }
    return promoted_total;
}

void rogue_vendor_scarcity_record(int material_def_index, int deficit_delta)
{
    if (material_def_index < 0 || material_def_index >= ROGUE_VENDOR_SCARCITY_CAP)
        return;
    int v = g_vendor_scarcity[material_def_index];
    long long nv = (long long) v + deficit_delta;
    if (nv < -100000)
        nv = -100000;
    if (nv > 100000)
        nv = 100000;
    g_vendor_scarcity[material_def_index] = (int) nv;
}
int rogue_vendor_scarcity_score(int material_def_index)
{
    if (material_def_index < 0 || material_def_index >= ROGUE_VENDOR_SCARCITY_CAP)
        return 0;
    return g_vendor_scarcity[material_def_index];
}
