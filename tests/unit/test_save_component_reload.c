#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Use real buff system */

/* We'll reload inventory component (id=3) after clearing runtime state; we can't easily inspect
 * internal items here, so rely on non-error path. */
int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("RELOAD_FAIL save\n");
        return 1;
    }
    int rc = rogue_save_reload_component_from_slot(0, 3);
    if (rc != 0)
    {
        printf("RELOAD_FAIL rc=%d\n", rc);
        return 1;
    }
    printf("RELOAD_OK comp=3\n");
    return 0;
}
