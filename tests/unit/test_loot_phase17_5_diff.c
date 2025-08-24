/* Unit test for Phase 17.5 inventory record-level diff metrics */
#include "../../src/core/app_state.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/save_manager.h"
#include <assert.h>
#include <stdio.h>

static void reset_items(void)
{
    for (int i = 0; i < g_app.item_instance_cap; i++)
    {
        g_app.item_instances[i].active = 0;
    }
}

int main(void)
{
    rogue_register_core_save_components();
    rogue_save_manager_init();
    rogue_save_set_incremental(1);
    reset_items();
    int a = rogue_items_spawn(0, 1, 0, 0);
    assert(a >= 0);
    int b = rogue_items_spawn(1, 2, 0, 0);
    assert(b >= 0);
    int rc = rogue_save_manager_save_slot(0);
    assert(rc == 0);
    unsigned reused = 123, rewritten = 0;
    rogue_save_inventory_diff_metrics(&reused, &rewritten);
    assert(rewritten == 2 && reused == 0);
    rc = rogue_save_manager_save_slot(0);
    assert(rc == 0);
    reused = 0;
    rewritten = 0;
    rogue_save_inventory_diff_metrics(&reused, &rewritten);
    assert(reused == 2 && rewritten == 0);
    RogueItemInstance* ia = (RogueItemInstance*) rogue_item_instance_at(a);
    assert(ia);
    ia->quantity = 5;
    rc = rogue_save_manager_save_slot(0);
    assert(rc == 0);
    reused = 0;
    rewritten = 0;
    rogue_save_inventory_diff_metrics(&reused, &rewritten);
    assert(reused == 1 && rewritten == 1);
    printf("Phase17.5 diff metrics test passed\n");
    return 0;
}
