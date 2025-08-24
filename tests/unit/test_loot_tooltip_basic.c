#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/loot/loot_tooltip.h"
#include "../../src/util/path_utils.h"
#include <stdio.h>
#include <string.h>

#include "../../src/core/app/app_state.h"
RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

int main(void)
{
    rogue_item_defs_reset();
    char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("TT_FAIL path\n");
        return 2;
    }
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("TT_FAIL load\n");
        return 3;
    }
    rogue_items_init_runtime();
    int axe_def = rogue_item_def_index("epic_axe");
    if (axe_def < 0)
    {
        printf("TT_FAIL def\n");
        return 4;
    }
    int inst = rogue_items_spawn(axe_def, 1, 5, 5);
    if (inst < 0)
    {
        printf("TT_FAIL spawn\n");
        return 5;
    }
    char buf[256];
    if (!rogue_item_tooltip_build(inst, buf, sizeof buf))
    {
        printf("TT_FAIL build\n");
        return 6;
    }
    if (strstr(buf, "Epic Axe") == NULL && strstr(buf, "EpIc Axe") == NULL)
    {
        printf("TT_FAIL name_missing '%s'\n", buf);
        return 7;
    }
    if (strstr(buf, "Damage:") == NULL)
    {
        printf("TT_FAIL dmg_missing '%s'\n", buf);
        return 8;
    }
    printf("TT_OK len=%zu\n", strlen(buf));
    return 0;
}
