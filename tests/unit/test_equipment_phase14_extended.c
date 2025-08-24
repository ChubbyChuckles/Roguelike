#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/game/stat_cache.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>
#include <string.h>

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
    if (!load_items())
    {
        printf("EQ14_FAIL load_items\n");
        return 10;
    }
    int sword = rogue_item_def_index("iron_sword");
    if (sword < 0)
    {
        printf("EQ14_FAIL sword_def\n");
        return 11;
    }
    int inst = rogue_items_spawn(sword, 1, 0, 0);
    if (inst < 0)
    {
        printf("EQ14_FAIL spawn\n");
        return 12;
    }
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, inst) != 0)
    {
        printf("EQ14_FAIL equip_weapon\n");
        return 13;
    }
    RoguePlayer p = {0};
    p.strength = 10;
    p.dexterity = 5;
    p.vitality = 8;
    p.intelligence = 3;
    p.max_health = 100;
    p.crit_chance = 5;
    p.crit_damage = 150;
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_update(&p);
    if (g_player_stat_cache.dps_estimate <= 0)
    {
        printf("EQ14_FAIL dps=%d\n", g_player_stat_cache.dps_estimate);
        return 14;
    }
    int base_ehp = g_player_stat_cache.ehp_estimate;
    /* Simulate equipping an armor piece by spawning generic armor def if present */
    int any_armor = -1;
    for (int i = 0; i < rogue_item_defs_count(); i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_ARMOR)
        {
            any_armor = i;
            break;
        }
    }
    if (any_armor >= 0)
    {
        int armor_inst = rogue_items_spawn(any_armor, 1, 0, 0);
        rogue_equip_try(ROGUE_EQUIP_ARMOR_CHEST, armor_inst);
        rogue_stat_cache_mark_dirty();
        rogue_stat_cache_update(&p);
        if (g_player_stat_cache.ehp_estimate <= base_ehp)
        {
            printf("EQ14_FAIL armor_ehp base=%d new=%d\n", base_ehp,
                   g_player_stat_cache.ehp_estimate);
            return 15;
        }
    }
    printf("EQ14_OK dps=%d ehp=%d\n", g_player_stat_cache.dps_estimate,
           g_player_stat_cache.ehp_estimate);
    return 0;
}
