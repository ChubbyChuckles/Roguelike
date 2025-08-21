#include "core/app_state.h"
#include "core/loot/loot_instances.h"
#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>

static int fail = 0;
#define CHECK(c, msg)                                                                              \
    do                                                                                             \
    {                                                                                              \
        if (!(c))                                                                                  \
        {                                                                                          \
            printf("FAIL:%s line %d %s\n", __FILE__, __LINE__, msg);                               \
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
    /* init loot runtime */ rogue_items_init_runtime();
    int inst = rogue_items_spawn(0, 3, 0, 0);
    if (inst < 0)
    {
        printf("FAIL:spawn\n");
        return 1;
    }
    g_app.item_instances[inst].enchant_level = 2; /* simulate enchant */
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("FAIL:full_save\n");
        return 1;
    }
    /* partial inventory-only save */
    g_app.item_instances[inst].quantity = 7;
    if (rogue_save_manager_save_slot_inventory_only(0) != 0)
    {
        printf("FAIL:inv_only_save\n");
        return 1;
    }
    /* backup rotate (no prune) */
    if (rogue_save_manager_backup_rotate(0, 3) != 0)
    {
        printf("FAIL:backup_rotate\n");
        return 1;
    }
    /* load slot and ensure quantity reflects inventory-only save change */
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("FAIL:load\n");
        return 1;
    }
    const RogueItemInstance* it = rogue_item_instance_at(inst);
    CHECK(it && it->quantity == 7, "quantity_updated");
    /* cleanup */ rogue_items_shutdown_runtime();
    if (fail)
    {
        printf("FAILURES\n");
        return 1;
    }
    printf("OK:save_phase15_inventory_partial_backup\n");
    return 0;
}
