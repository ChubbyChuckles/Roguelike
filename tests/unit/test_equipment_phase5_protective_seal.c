#include "../../src/core/equipment/equipment_enchant.h"
#include "../../src/core/inventory/inventory.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/vendor/economy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void load_items()
{
    rogue_item_defs_reset();
    int ok = rogue_item_defs_load_from_cfg("assets/test_items.cfg");
    if (ok <= 0)
        ok = rogue_item_defs_load_from_cfg("../assets/test_items.cfg");
    assert(ok > 0);
}

static int spawn_item()
{
    int def = rogue_item_def_index("long_sword");
    assert(def >= 0);
    return rogue_items_spawn(def, 1, 0, 0);
}

int main(void)
{
    load_items();
    /* add materials */
    int seal = rogue_item_def_index("protective_seal");
    assert(seal >= 0);
    rogue_inventory_add(seal, 5);
    int orb = rogue_item_def_index("enchant_orb");
    assert(orb >= 0);
    rogue_inventory_add(orb, 5);
    rogue_econ_reset();
    rogue_econ_add_gold(100000);
    int inst = spawn_item();
    assert(inst >= 0);
    /* Seed affixes manually */
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
    assert(it);
    it->rarity = 3;
    unsigned int rng = 42;
    it->prefix_index = rogue_affix_roll(ROGUE_AFFIX_PREFIX, it->rarity, &rng);
    it->prefix_value = rogue_affix_roll_value(it->prefix_index, &rng);
    it->suffix_index = rogue_affix_roll(ROGUE_AFFIX_SUFFIX, it->rarity, &rng);
    it->suffix_value = rogue_affix_roll_value(it->suffix_index, &rng);
    int old_prefix = it->prefix_index, old_suffix = it->suffix_index;
    /* Apply seal locking prefix only */
    int rc = rogue_item_instance_apply_protective_seal(inst, 1, 0);
    assert(rc == 0);
    assert(rogue_item_instance_is_prefix_locked(inst));
    int cost = 0;
    rc = rogue_item_instance_enchant(inst, 1, 1, &cost);
    assert(rc == 0); /* prefix reroll ignored due to lock; suffix rerolled */
    assert(it->prefix_index == old_prefix);
    assert(it->suffix_index != old_suffix);
    old_suffix = it->suffix_index;
    /* Lock suffix too */
    rc = rogue_item_instance_apply_protective_seal(inst, 0, 1);
    assert(rc == 0);
    assert(rogue_item_instance_is_suffix_locked(inst));
    rc = rogue_item_instance_enchant(inst, 1, 1, &cost);
    assert(rc == -2); /* nothing to reroll (both locked) */
    printf("equipment_phase5_protective_seal_ok\n");
    return 0;
}
