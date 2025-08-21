#include "../../src/core/loot/loot_tooltip.h"
#include "core/app_state.h"
#include "core/equipment/equipment.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include <stdio.h>
#include <string.h>
RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("TTC_FAIL path\n");
        return 2;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("TTC_FAIL load\n");
        return 3;
    }
    rogue_items_init_runtime();
    rogue_equip_reset();
    int sword = rogue_item_def_index("long_sword");
    int staff = rogue_item_def_index("magic_staff");
    if (sword < 0 || staff < 0)
    {
        printf("TTC_FAIL defs\n");
        return 4;
    }
    int inst_sword = rogue_items_spawn(sword, 1, 1, 1);
    int inst_staff = rogue_items_spawn(staff, 1, 2, 2);
    if (inst_sword < 0 || inst_staff < 0)
    {
        printf("TTC_FAIL spawn\n");
        return 5;
    }
    if (rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_sword) != 0)
    {
        printf("TTC_FAIL equip\n");
        return 6;
    }
    char buf[512];
    if (!rogue_item_tooltip_build_compare(inst_staff, ROGUE_EQUIP_WEAPON, buf, sizeof buf))
    {
        printf("TTC_FAIL build\n");
        return 7;
    }
    if (strstr(buf, "Compared to equipped") == NULL)
    {
        printf("TTC_FAIL compare_line_missing '%s'\n", buf);
        return 8;
    }
    printf("TTC_OK len=%zu\n", strlen(buf));
    return 0;
}
