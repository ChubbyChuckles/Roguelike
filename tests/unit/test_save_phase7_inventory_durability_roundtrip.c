#include "core/app_state.h"
#include "core/loot/loot_instances.h"
#include "core/loot/loot_item_defs.h"
#include "core/path_utils.h"
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(c, msg)                                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL:%s %d %s\n", __FILE__, __LINE__, msg);                                    \
            fail = 1;                                                                              \
        }                                                                                          \
    } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    extern void rogue_register_core_save_components(void);
    rogue_register_core_save_components();
    /* load item defs */ char pitems[256];
    if (!rogue_find_asset_path("test_items.cfg", pitems, sizeof pitems))
    {
        printf("FAIL:find items\n");
        return 2;
    }
    rogue_item_defs_reset();
    if (rogue_item_defs_load_from_cfg(pitems) <= 0)
    {
        printf("FAIL:load items\n");
        return 3;
    }
    rogue_items_init_runtime();
    /* spawn two weapon instances with mutated durability */ int instA =
        rogue_items_spawn(0, 1, 0, 0);
    int instB = rogue_items_spawn(1, 1, 0, 0);
    if (instA < 0 || instB < 0)
    {
        printf("FAIL:spawn\n");
        return 4;
    }
    /* damage durability */ rogue_item_instance_damage_durability(instA, 5);
    rogue_item_instance_damage_durability(instB, 10);
    int curA0, maxA0;
    rogue_item_instance_get_durability(instA, &curA0, &maxA0);
    int curB0, maxB0;
    rogue_item_instance_get_durability(instB, &curB0, &maxB0);
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("FAIL:save\n");
        return 5;
    }
    /* wipe runtime loot */ rogue_items_shutdown_runtime();
    rogue_items_init_runtime();
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("FAIL:load\n");
        return 6;
    }
    /* after load, we expect two active instances with identical durability values */ int active =
        0;
    for (int i = 0; i < g_app.item_instance_cap; i++)
        if (g_app.item_instances[i].active)
            active++;
    CHECK(active == 2, "count");
    int found = 0;
    for (int i = 0; i < g_app.item_instance_cap; i++)
        if (g_app.item_instances[i].active)
        {
            int c, m;
            rogue_item_instance_get_durability(i, &c, &m);
            if ((c == curA0 && m == maxA0) || (c == curB0 && m == maxB0))
                found++;
        }
    CHECK(found == 2, "durabilities");
    if (fail)
    {
        printf("FAILURES\n");
        return 1;
    }
    printf("OK:save_phase7_inventory_durability_roundtrip\n");
    return 0;
}
