/* Phase 16.6: Analyzer malformed / boundary tests & extended validator coverage */
#include "../../src/core/equipment/equipment_budget_analyzer.h"
#include "../../src/core/equipment/equipment_content.h"
#include "../../src/core/loot/loot_instances.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_true(int cond, const char* msg)
{
    if (!cond)
    {
        printf("FAIL:%s\n", msg);
        exit(1);
    }
}

static void test_empty_inventory()
{
    rogue_budget_analyzer_reset();
    RogueBudgetReport rep;
    rogue_budget_analyzer_run(&rep);
    assert_true(rep.item_count >= 0, "empty item_count non-negative");
    char buf[128];
    int n = rogue_budget_analyzer_export_json(buf, sizeof buf);
    assert_true(n >= 0, "json export non-negative");
    assert_true(strstr(buf, "item_count") != NULL, "json has item_count field");
}

static void test_extreme_item_level()
{
    rogue_budget_analyzer_reset();
    int idx = rogue_items_spawn(0, 1, 0, 0);
    assert_true(idx >= 0, "spawn item");
    const RogueItemInstance* itc = rogue_item_instance_at(idx);
    RogueItemInstance* it = (RogueItemInstance*) itc;
    it->item_level = 500;
    it->rarity = 4;
    it->prefix_index = 0;
    it->prefix_value = 1000;
    it->suffix_index = -1;
    it->suffix_value = 0;
    RogueBudgetReport rep;
    rogue_budget_analyzer_run(&rep);
    assert_true(rep.item_count >= 1, "extreme level counted");
}

static void test_runeword_validator_boundaries()
{
    assert_true(rogue_runeword_validate_pattern("abc_def_ghi") == 0, "max length pattern ok");
    assert_true(rogue_runeword_validate_pattern("abcd_def_ghi") < 0, "too long pattern rejected");
    assert_true(rogue_runeword_validate_pattern("a_b_c_d_e") == 0, "max segments ok");
    assert_true(rogue_runeword_validate_pattern("a_b_c_d_e_f") < 0, "segment overflow rejected");
}

int main()
{
    test_empty_inventory();
    test_extreme_item_level();
    test_runeword_validator_boundaries();
    printf("Phase16.6 analyzer boundary + validator edge tests OK\n");
    return 0;
}
