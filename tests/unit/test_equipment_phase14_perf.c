#include "core/equipment/equipment.h"
#include "core/equipment/equipment_perf.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/stat_cache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* prototypes already provided by included headers; avoid redefinition */

static int load_items()
{
    char p[256];
    if (!rogue_find_asset_path("items/swords.cfg", p, sizeof p))
        return 0;
    for (int i = (int) strlen(p) - 1; i >= 0; i--)
    {
        if (p[i] == '/' || p[i] == '\\')
        {
            p[i] = '\0';
            break;
        }
    }
    rogue_item_defs_reset();
    return rogue_item_defs_load_directory(p) > 0;
}

int main(void)
{
    assert(load_items());
    /* spawn several items across slots */
    int defs_spawned = 0;
    for (int i = 0; i < rogue_item_defs_count() && defs_spawned < ROGUE_EQUIP__COUNT; i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (!d)
            continue;
        int inst = rogue_items_spawn(i, 1, 0, 0);
        if (inst < 0)
            continue; /* naive slot mapping: weapon->weapon first, rest to armor slots */
        if (d->category == ROGUE_ITEM_WEAPON)
        {
            rogue_equip_try(ROGUE_EQUIP_WEAPON, inst);
        }
        else
        {
            int target = -1;
            for (int s = ROGUE_EQUIP_ARMOR_HEAD; s <= ROGUE_EQUIP_ARMOR_FEET; s++)
            {
                if (rogue_equip_get((enum RogueEquipSlot) s) < 0)
                {
                    target = s;
                    break;
                }
            }
            if (target >= 0)
                rogue_equip_try((enum RogueEquipSlot) target, inst);
        }
        defs_spawned++;
    }
    rogue_equip_profiler_reset();
    rogue_equipment_aggregate(ROGUE_EQUIP_AGGREGATE_SCALAR);
    int scalar_str = g_equip_total_strength;
    int scalar_arm = g_equip_total_armor;
    rogue_equipment_aggregate(ROGUE_EQUIP_AGGREGATE_SIMD);
    assert(g_equip_total_strength == scalar_str);
    assert(g_equip_total_armor == scalar_arm);
    /* Ensure profiler captured both zones */
    double t_scalar = 0, t_simd = 0;
    int c_scalar = 0, c_simd = 0;
    assert(rogue_equip_profiler_zone_stats("agg_scalar", &t_scalar, &c_scalar) == 0);
    assert(rogue_equip_profiler_zone_stats("agg_simd", &t_simd, &c_simd) == 0);
    assert(c_scalar == 1 && c_simd == 1);
    /* Arena high water should stay small (no dynamic allocations here but check API) */
    assert(rogue_equip_frame_high_water() <= rogue_equip_frame_capacity());
    printf("EQ14_PERF_OK strength=%d armor=%d scalar_ms=%.3f simd_ms=%.3f\n", scalar_str,
           scalar_arm, t_scalar, t_simd);
    return 0;
}
