#include "../../src/core/inventory/inventory_entries.h"
#include "../../src/core/inventory/inventory_tag_rules.h"
#include "../../src/core/inventory/inventory_tags.h"
#include "../../src/core/save_manager.h"
#include <assert.h>
#include <stdio.h>

static void test_rule_application_basic()
{
    rogue_inventory_entries_init();
    rogue_inv_tags_init();
    rogue_inv_tag_rules_clear();
    /* Rule: rarity >=2 (min=2,max=255) any category => tag "HighValue" color red */
    assert(rogue_inv_tag_rules_add(2, 0xFF, 0, "HighValue", 0xFF0000FF) == 0);
    /* We cannot assert actual rarity without accessing item defs; this ensures no crash and tag
     * attempt executed on pickup. */
    rogue_inventory_register_pickup(5, 1);
    rogue_inv_tag_rules_apply_def(5);
}

static void test_persistence()
{
    rogue_inventory_entries_init();
    rogue_inv_tags_init();
    rogue_inv_tag_rules_clear();
    rogue_register_core_save_components();
    assert(rogue_inv_tag_rules_add(1, 3, 0, "Mid", 0x00FF00FF) == 0);
    assert(rogue_inv_tag_rules_add(4, 0xFF, 0, "End", 0x0000FFFF) == 0);
    assert(rogue_save_manager_save_slot(0) == 0);
    rogue_inv_tag_rules_clear();
    assert(rogue_inv_tag_rules_count() == 0);
    assert(rogue_save_manager_load_slot(0) == 0);
    assert(rogue_inv_tag_rules_count() == 2);
    const RogueInvTagRule* r0 = rogue_inv_tag_rules_get(0);
    const RogueInvTagRule* r1 = rogue_inv_tag_rules_get(1);
    assert(r0 && r1);
}

int main()
{
    test_rule_application_basic();
    test_persistence();
    printf("inventory_phase3_rules: OK\n");
    return 0;
}
