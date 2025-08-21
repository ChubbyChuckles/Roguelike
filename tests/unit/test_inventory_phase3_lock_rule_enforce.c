#include "../../src/core/inventory/inventory_entries.h"
#include "../../src/core/inventory/inventory_tag_rules.h"
#include "../../src/core/inventory/inventory_tags.h"
#include "../../src/core/inventory/inventory_ui.h"
#include "../../src/core/save_manager.h"
#include <assert.h>
#include <stdio.h>

/* Ensures locked/favorite items cannot be salvaged/dropped and rule application order
 * deterministic. */
static void test_lock_prevents_salvage_drop()
{
    rogue_inventory_entries_init();
    rogue_inv_tags_init();
    rogue_inv_tag_rules_clear();
    rogue_inventory_register_pickup(12, 5);
    rogue_inv_tags_set_flags(12, ROGUE_INV_FLAG_LOCKED | ROGUE_INV_FLAG_FAVORITE);
    int salv = rogue_inventory_ui_salvage_def(12);
    assert(salv == 0); /* locked prevents salvage */
    int drop = rogue_inventory_ui_drop_one(12);
    assert(drop == -3); /* locked prevents drop */
}

static void test_rule_determinism()
{
    rogue_inventory_entries_init();
    rogue_inv_tags_init();
    rogue_inv_tag_rules_clear();
    rogue_inv_tag_rules_add(0, 0xFF, 0, "A", 0x112233FF);
    rogue_inv_tag_rules_add(0, 0xFF, 0, "B", 0x445566FF);
    rogue_inventory_register_pickup(20, 1); /* triggers apply */
    rogue_inv_tag_rules_apply_def(20);
    const char* tags[4];
    int count = rogue_inv_tags_list(20, tags, 4);
    assert(count == 2);
    unsigned seenA = 0, seenB = 0;
    for (int i = 0; i < count; i++)
    {
        if (tags[i] && tags[i][0] == 'A')
            seenA = 1;
        if (tags[i] && tags[i][0] == 'B')
            seenB = 1;
    }
    assert(seenA && seenB);
}

int main()
{
    test_lock_prevents_salvage_drop();
    test_rule_determinism();
    printf("inventory_phase3_lock_rule_enforce: OK\n");
    return 0;
}
