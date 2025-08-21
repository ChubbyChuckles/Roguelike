#include "core/equipment/equipment.h"
#include "core/equipment/equipment_stats.h"
#include "core/loot/loot_affixes.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include "core/stat_cache.h"
#include "entities/player.h"
#include <assert.h>
#include <stdio.h>

int main()
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("STAT_CACHE_FAIL items\n");
        return 1;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("STAT_CACHE_FAIL load_items\n");
        return 1;
    }
    rogue_affixes_reset();
    char paff[256];
    if (!rogue_find_asset_path("affixes.cfg", paff, sizeof paff))
    {
        printf("STAT_CACHE_FAIL afffind\n");
        return 2;
    }
    if (rogue_affixes_load_from_cfg(paff) <= 0)
    {
        printf("STAT_CACHE_FAIL affload\n");
        return 2;
    }
    rogue_items_init_runtime();
    RoguePlayer p;
    rogue_player_init(&p);
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_update(&p);
    int base_dps = g_player_stat_cache.dps_estimate;
    int base_ehp = g_player_stat_cache.ehp_estimate;
    if (base_dps <= 0 || base_ehp <= 0)
    {
        printf("STAT_CACHE_FAIL basevals dps=%d ehp=%d\n", base_dps, base_ehp);
        return 3;
    }
    int def_index = rogue_item_def_index("long_sword");
    assert(def_index >= 0);
    int inst = rogue_items_spawn(def_index, 1, 0, 0);
    assert(inst >= 0);
    rogue_equip_reset();
    int rc = rogue_equip_try(ROGUE_EQUIP_WEAPON, inst);
    if (rc != 0)
    {
        printf("STAT_CACHE_FAIL equip rc=%d\n", rc);
        return 4;
    }
    rogue_equipment_apply_stat_bonuses(&p); /* marks dirty */
    rogue_stat_cache_update(&p);
    if (g_player_stat_cache.dps_estimate <= base_dps)
    {
        printf("STAT_CACHE_FAIL dps not increased %d -> %d\n", base_dps,
               g_player_stat_cache.dps_estimate);
        return 5;
    }
    printf("STAT_CACHE_OK base_dps=%d new_dps=%d base_ehp=%d ehp=%d\n", base_dps,
           g_player_stat_cache.dps_estimate, base_ehp, g_player_stat_cache.ehp_estimate);
    return 0;
}
