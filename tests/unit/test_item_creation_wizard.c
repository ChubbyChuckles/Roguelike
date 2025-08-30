#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/core/loot/item_debug.h"
#include "../../src/core/loot/loot_item_defs.h"

/* Phase 10.4: Validate item creation API: unique id, required fields, sanitation */
int main(void)
{
    rogue_item_defs_reset();

    /* Create a minimal valid item */
    const char* id = "wiz_test_sword";
    const char* name = "Wizard Test Sword";
    int idx = rogue_item_debug_create(id, name, ROGUE_ITEM_WEAPON, 5, 1, 123, 2, 4, 0, 2, 0, 0);
    assert(idx >= 0);

    /* Creating again with same id should fail with -2 */
    int dup = rogue_item_debug_create(id, name, ROGUE_ITEM_WEAPON, 5, 1, 123, 2, 4, 0, 2, 0, 0);
    assert(dup == -2);

    /* Sanitation: stack_max>=1, sockets clamped and max>=min */
    int idx2 =
        rogue_item_debug_create("wiz_bad", "Bad", ROGUE_ITEM_ARMOR, 1, -5, 10, 0, 0, 3, 0, -1, 10);
    assert(idx2 >= 0);
    const RogueItemDef* d2 = rogue_item_debug_get(idx2);
    assert(d2);
    assert(d2->stack_max >= 1);
    assert(d2->socket_min == 0);
    assert(d2->socket_max >= d2->socket_min && d2->socket_max <= 6);

    printf("OK test_item_creation_wizard idx=%d idx2=%d count=%d\n", idx, idx2,
           rogue_item_debug_count());
    return 0;
}
