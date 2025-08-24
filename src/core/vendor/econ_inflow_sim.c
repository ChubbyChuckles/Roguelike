#include "econ_inflow_sim.h"
#include "../loot/loot_item_defs.h"
#include "econ_materials.h"
#include "econ_value.h"

int rogue_econ_inflow_baseline(int kills_per_min, double hours, double avg_item_drops_per_kill,
                               double avg_material_drops_per_kill, RogueEconInflowResult* out)
{
    if (!out)
        return 0;
    if (kills_per_min < 0 || hours <= 0.0)
        return 0;
    if (avg_item_drops_per_kill < 0.0 || avg_material_drops_per_kill < 0.0)
        return 0;
    int total_defs = rogue_item_defs_count();
    if (total_defs <= 0)
        return 0;
    if (rogue_econ_material_catalog_count() <= 0)
    { /* attempt lazy build */
        rogue_econ_material_catalog_build();
    }
    /* Simplistic baseline: average value per non-material item = arithmetic mean of base value over
     * non-material defs. */
    double sum_item_base = 0.0;
    int item_count = 0;
    for (int i = 0; i < total_defs; i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        if (d->category == ROGUE_ITEM_MATERIAL)
            continue;
        if (d->base_value > 0)
        {
            sum_item_base += (double) d->base_value;
            item_count++;
        }
    }
    double avg_item_base = (item_count > 0) ? (sum_item_base / (double) item_count) : 1.0;
    /* Average material base value: sample catalog entries. */
    double sum_mat_base = 0.0;
    int mat_entries = rogue_econ_material_catalog_count();
    for (int i = 0; i < mat_entries; i++)
    {
        const RogueMaterialEntry* e = rogue_econ_material_catalog_get(i);
        if (!e)
            continue;
        if (e->base_value > 0)
            sum_mat_base += (double) e->base_value;
    }
    double avg_mat_base = (mat_entries > 0) ? (sum_mat_base / (double) mat_entries) : 1.0;

    double total_minutes = hours * 60.0;
    double expected_kills = total_minutes * (double) kills_per_min;
    double expected_items = expected_kills * avg_item_drops_per_kill;
    double expected_materials = expected_kills * avg_material_drops_per_kill;
    double expected_item_value = expected_items * avg_item_base;
    double expected_material_value = expected_materials * avg_mat_base;

    out->hours = hours;
    out->kills_per_min = kills_per_min;
    out->avg_item_drops_per_kill = avg_item_drops_per_kill;
    out->avg_material_drops_per_kill = avg_material_drops_per_kill;
    out->expected_items = expected_items;
    out->expected_materials = expected_materials;
    out->expected_item_value = expected_item_value;
    out->expected_material_value = expected_material_value;
    out->expected_total_value = expected_item_value + expected_material_value;
    return 1;
}
