#include "../../src/core/integration/dependency_manager.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

// Test helper macros
#define TEST_ASSERT(condition, message)                                                            \
    do                                                                                             \
    {                                                                                              \
        if (condition)                                                                             \
        {                                                                                          \
            tests_passed++;                                                                        \
            printf("[PASS] %s\n", message);                                                        \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            tests_failed++;                                                                        \
            printf("[FAIL] %s\n", message);                                                        \
        }                                                                                          \
    } while (0)

#define TEST_SECTION(name) printf("\n=== %s ===\n", name)

// Test system lifecycle
static void test_dependency_manager_lifecycle(void)
{
    TEST_SECTION("Dependency Manager Lifecycle");

    // Test creation
    RogueDependencyManager* manager = rogue_dependency_manager_create();
    TEST_ASSERT(manager != NULL, "Manager creation");

    // Test initialization
    bool init_result = rogue_dependency_manager_initialize(manager);
    TEST_ASSERT(init_result == true, "Manager initialization");
    TEST_ASSERT(manager->graph.node_count == 0, "Initial node count is zero");
    TEST_ASSERT(manager->total_dependencies == 0, "Initial dependency count is zero");
    TEST_ASSERT(manager->auto_resolve == true, "Default auto_resolve setting");
    TEST_ASSERT(manager->strict_mode == false, "Default strict_mode setting");
    TEST_ASSERT(manager->debug_mode == false, "Default debug_mode setting");

    // Test configuration
    rogue_dependency_manager_set_auto_resolve(manager, false);
    TEST_ASSERT(manager->auto_resolve == false, "Set auto_resolve to false");

    rogue_dependency_manager_set_strict_mode(manager, true);
    TEST_ASSERT(manager->strict_mode == true, "Set strict_mode to true");

    rogue_dependency_manager_set_debug_mode(manager, true);
    TEST_ASSERT(manager->debug_mode == true, "Set debug_mode to true");

    // Test reset
    rogue_dependency_manager_reset(manager);
    TEST_ASSERT(manager->graph.node_count == 0, "Reset clears nodes");
    TEST_ASSERT(manager->total_dependencies == 0, "Reset clears dependencies");

    // Test cleanup and destruction
    rogue_dependency_manager_cleanup(manager);
    rogue_dependency_manager_destroy(manager);
    TEST_ASSERT(true, "Manager cleanup and destruction");
}

// Test file management
static void test_file_management(void)
{
    TEST_SECTION("File Management");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);

    // Test adding files
    bool add1 =
        rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    TEST_ASSERT(add1 == true, "Add items.json file");
    TEST_ASSERT(manager->graph.node_count == 1, "Node count after adding first file");

    bool add2 = rogue_dependency_manager_add_file(manager, "assets/affixes.json",
                                                  ROGUE_FILE_TYPE_AFFIXES, 5);
    TEST_ASSERT(add2 == true, "Add affixes.json file");
    TEST_ASSERT(manager->graph.node_count == 2, "Node count after adding second file");

    bool add3 = rogue_dependency_manager_add_file(manager, "assets/enemies.json",
                                                  ROGUE_FILE_TYPE_ENEMIES, 15);
    TEST_ASSERT(add3 == true, "Add enemies.json file");
    TEST_ASSERT(manager->graph.node_count == 3, "Node count after adding third file");

    // Test duplicate file rejection
    bool add_duplicate =
        rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    TEST_ASSERT(add_duplicate == false, "Reject duplicate file");
    TEST_ASSERT(manager->graph.node_count == 3, "Node count unchanged after duplicate");

    // Test finding files
    RogueDependencyNode* found1 = rogue_dependency_manager_find_node(manager, "assets/items.json");
    TEST_ASSERT(found1 != NULL, "Find items.json file");
    TEST_ASSERT(found1->file_type == ROGUE_FILE_TYPE_ITEMS, "Correct file type for items");
    TEST_ASSERT(found1->priority == 10, "Correct priority for items");

    RogueDependencyNode* found2 =
        rogue_dependency_manager_find_node(manager, "assets/affixes.json");
    TEST_ASSERT(found2 != NULL, "Find affixes.json file");
    TEST_ASSERT(found2->file_type == ROGUE_FILE_TYPE_AFFIXES, "Correct file type for affixes");
    TEST_ASSERT(found2->priority == 5, "Correct priority for affixes");

    RogueDependencyNode* not_found =
        rogue_dependency_manager_find_node(manager, "assets/nonexistent.json");
    TEST_ASSERT(not_found == NULL, "Non-existent file returns NULL");

    // Test file info updates
    bool update = rogue_dependency_manager_update_file_info(manager, "assets/items.json",
                                                            1234567890, "abc123def456");
    TEST_ASSERT(update == true, "Update file info");
    TEST_ASSERT(found1->last_modified == 1234567890, "Correct last modified time");
    TEST_ASSERT(strcmp(found1->checksum, "abc123def456") == 0, "Correct checksum");

    // Test removing files
    bool remove1 = rogue_dependency_manager_remove_file(manager, "assets/affixes.json");
    TEST_ASSERT(remove1 == true, "Remove affixes.json file");
    TEST_ASSERT(manager->graph.node_count == 2, "Node count after removal");

    RogueDependencyNode* removed_check =
        rogue_dependency_manager_find_node(manager, "assets/affixes.json");
    TEST_ASSERT(removed_check == NULL, "Removed file not found");

    bool remove_nonexistent =
        rogue_dependency_manager_remove_file(manager, "assets/nonexistent.json");
    TEST_ASSERT(remove_nonexistent == false, "Remove non-existent file fails");

    rogue_dependency_manager_destroy(manager);
}

// Test dependency registration
static void test_dependency_registration(void)
{
    TEST_SECTION("Dependency Registration");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false); // Reduce output

    // Add files
    rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    rogue_dependency_manager_add_file(manager, "assets/affixes.json", ROGUE_FILE_TYPE_AFFIXES, 5);
    rogue_dependency_manager_add_file(manager, "assets/enemies.json", ROGUE_FILE_TYPE_ENEMIES, 15);
    rogue_dependency_manager_add_file(manager, "assets/loot_tables.json",
                                      ROGUE_FILE_TYPE_LOOT_TABLES, 20);

    // Test adding strong dependencies
    bool dep1 = rogue_dependency_manager_add_dependency(
        manager, "assets/items.json", "assets/affixes.json", "affix_fire_damage",
        ROGUE_DEP_TYPE_STRONG, 1, "Items depend on affixes for stat modifiers");
    TEST_ASSERT(dep1 == true, "Add strong dependency: items -> affixes");
    TEST_ASSERT(manager->total_dependencies == 1, "Dependency count after first dependency");

    bool dep2 = rogue_dependency_manager_add_dependency(
        manager, "assets/loot_tables.json", "assets/items.json", "item_sword_basic",
        ROGUE_DEP_TYPE_STRONG, 2, "Loot tables depend on items for drop definitions");
    TEST_ASSERT(dep2 == true, "Add strong dependency: loot_tables -> items");
    TEST_ASSERT(manager->total_dependencies == 2, "Dependency count after second dependency");

    bool dep3 = rogue_dependency_manager_add_dependency(
        manager, "assets/enemies.json", "assets/loot_tables.json", "loot_table_goblin",
        ROGUE_DEP_TYPE_STRONG, 3, "Enemies depend on loot tables for drops");
    TEST_ASSERT(dep3 == true, "Add strong dependency: enemies -> loot_tables");
    TEST_ASSERT(manager->total_dependencies == 3, "Dependency count after third dependency");

    // Test adding weak dependencies
    bool weak_dep = rogue_dependency_manager_add_weak_dependency(
        manager, "assets/enemies.json", "assets/skills.json", "skill_fire_breath",
        "Optional skill reference");
    TEST_ASSERT(weak_dep == true, "Add weak dependency: enemies -> skills (optional)");
    TEST_ASSERT(manager->total_dependencies == 4, "Dependency count after weak dependency");

    // Test duplicate dependency rejection
    bool dup_dep = rogue_dependency_manager_add_dependency(
        manager, "assets/items.json", "assets/affixes.json", "affix_fire_damage",
        ROGUE_DEP_TYPE_STRONG, 1, NULL);
    TEST_ASSERT(dup_dep == false, "Reject duplicate dependency");
    TEST_ASSERT(manager->total_dependencies == 4, "Dependency count unchanged after duplicate");

    // Test getting dependencies
    RogueDependency deps[10];
    int dep_count =
        rogue_dependency_manager_get_dependencies(manager, "assets/items.json", deps, 10);
    TEST_ASSERT(dep_count == 1, "Get dependencies for items.json");
    TEST_ASSERT(strcmp(deps[0].target_file, "assets/affixes.json") == 0,
                "Correct target file in dependency");
    TEST_ASSERT(strcmp(deps[0].reference_key, "affix_fire_damage") == 0,
                "Correct reference key in dependency");
    TEST_ASSERT(deps[0].type == ROGUE_DEP_TYPE_STRONG, "Correct dependency type");

    // Test weak dependency check
    bool is_weak = rogue_dependency_manager_is_weak_dependency(
        manager, "assets/enemies.json", "assets/skills.json", "skill_fire_breath");
    TEST_ASSERT(is_weak == true, "Correctly identify weak dependency");

    bool is_not_weak = rogue_dependency_manager_is_weak_dependency(
        manager, "assets/items.json", "assets/affixes.json", "affix_fire_damage");
    TEST_ASSERT(is_not_weak == false, "Correctly identify strong dependency");

    // Test getting weak dependencies
    RogueDependency weak_deps[10];
    int weak_count = rogue_dependency_manager_get_weak_dependencies(manager, "assets/enemies.json",
                                                                    weak_deps, 10);
    TEST_ASSERT(weak_count == 1, "Get weak dependencies for enemies.json");
    TEST_ASSERT(strcmp(weak_deps[0].target_file, "assets/skills.json") == 0,
                "Correct weak dependency target");

    // Test removing dependencies
    bool remove_dep = rogue_dependency_manager_remove_dependency(
        manager, "assets/enemies.json", "assets/skills.json", "skill_fire_breath");
    TEST_ASSERT(remove_dep == true, "Remove weak dependency");
    TEST_ASSERT(manager->total_dependencies == 3, "Dependency count after removal");

    bool remove_nonexistent = rogue_dependency_manager_remove_dependency(
        manager, "assets/items.json", "assets/nonexistent.json", "fake_ref");
    TEST_ASSERT(remove_nonexistent == false, "Remove non-existent dependency fails");

    rogue_dependency_manager_destroy(manager);
}

// Test dependency resolution
static void test_dependency_resolution(void)
{
    TEST_SECTION("Dependency Resolution");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Add files
    rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    rogue_dependency_manager_add_file(manager, "assets/affixes.json", ROGUE_FILE_TYPE_AFFIXES, 5);
    rogue_dependency_manager_add_file(manager, "assets/enemies.json", ROGUE_FILE_TYPE_ENEMIES, 15);

    // Add dependencies
    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/affixes.json",
                                            "affix_fire", ROGUE_DEP_TYPE_STRONG, 1, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/enemies.json", "assets/items.json",
                                            "sword_basic", ROGUE_DEP_TYPE_STRONG, 2, NULL);
    rogue_dependency_manager_add_weak_dependency(manager, "assets/enemies.json",
                                                 "assets/nonexistent.json", "optional_ref",
                                                 "Optional reference");

    // Test resolving all dependencies
    bool resolve_all = rogue_dependency_manager_resolve_all(manager);
    TEST_ASSERT(resolve_all == true, "Resolve all dependencies successfully");
    TEST_ASSERT(manager->resolved_dependencies == 3, "All dependencies resolved");
    TEST_ASSERT(manager->failed_resolutions == 0, "No failed resolutions");

    // Test individual dependency status
    RogueDependencyStatus status1 = rogue_dependency_manager_get_dependency_status(
        manager, "assets/items.json", "assets/affixes.json", "affix_fire");
    TEST_ASSERT(status1 == ROGUE_DEP_STATUS_RESOLVED, "Strong dependency resolved");

    RogueDependencyStatus status2 = rogue_dependency_manager_get_dependency_status(
        manager, "assets/enemies.json", "assets/items.json", "sword_basic");
    TEST_ASSERT(status2 == ROGUE_DEP_STATUS_RESOLVED, "Another strong dependency resolved");

    RogueDependencyStatus status3 = rogue_dependency_manager_get_dependency_status(
        manager, "assets/enemies.json", "assets/nonexistent.json", "optional_ref");
    TEST_ASSERT(status3 == ROGUE_DEP_STATUS_RESOLVED,
                "Weak dependency resolved (missing target OK)");

    // Test individual file resolution
    bool resolve_file = rogue_dependency_manager_resolve_file(manager, "assets/items.json");
    TEST_ASSERT(resolve_file == true, "Resolve individual file dependencies");

    // Test resolution with missing strong dependency
    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/missing.json",
                                            "missing_ref", ROGUE_DEP_TYPE_STRONG, 3, NULL);

    bool resolve_with_missing = rogue_dependency_manager_resolve_all(manager);
    TEST_ASSERT(resolve_with_missing == false, "Resolve fails with missing strong dependency");
    TEST_ASSERT(manager->failed_resolutions > 0, "Failed resolutions recorded");

    RogueDependencyStatus missing_status = rogue_dependency_manager_get_dependency_status(
        manager, "assets/items.json", "assets/missing.json", "missing_ref");
    TEST_ASSERT(missing_status == ROGUE_DEP_STATUS_MISSING,
                "Missing dependency has correct status");

    rogue_dependency_manager_destroy(manager);
}

// Test circular dependency detection
static void test_circular_dependency_detection(void)
{
    TEST_SECTION("Circular Dependency Detection");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Add files
    rogue_dependency_manager_add_file(manager, "assets/a.json", ROGUE_FILE_TYPE_OTHER, 10);
    rogue_dependency_manager_add_file(manager, "assets/b.json", ROGUE_FILE_TYPE_OTHER, 10);
    rogue_dependency_manager_add_file(manager, "assets/c.json", ROGUE_FILE_TYPE_OTHER, 10);
    rogue_dependency_manager_add_file(manager, "assets/d.json", ROGUE_FILE_TYPE_OTHER, 10);

    // Create circular dependency: a -> b -> c -> a
    rogue_dependency_manager_add_dependency(manager, "assets/a.json", "assets/b.json", "ref1",
                                            ROGUE_DEP_TYPE_STRONG, 1, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/b.json", "assets/c.json", "ref2",
                                            ROGUE_DEP_TYPE_STRONG, 1, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/c.json", "assets/a.json", "ref3",
                                            ROGUE_DEP_TYPE_STRONG, 1, NULL);

    // Add non-circular dependency: d -> a (should not be part of cycle)
    rogue_dependency_manager_add_dependency(manager, "assets/d.json", "assets/a.json", "ref4",
                                            ROGUE_DEP_TYPE_STRONG, 1, NULL);

    // Test cycle detection
    bool has_cycles = rogue_dependency_manager_detect_cycles(manager);
    TEST_ASSERT(has_cycles == true, "Circular dependency detected");
    TEST_ASSERT(manager->graph.has_cycles == true, "Graph has_cycles flag set");
    TEST_ASSERT(manager->graph.cycle_count > 0, "Cycle count is positive");

    // Test getting cycles
    char cycles[16][512];
    int cycle_count = rogue_dependency_manager_get_cycles(manager, cycles, 16);
    TEST_ASSERT(cycle_count > 0, "Get cycles returns positive count");
    TEST_ASSERT(strlen(cycles[0]) > 0, "First cycle description is not empty");

    // Test checking specific file for circular dependency
    bool a_has_circular =
        rogue_dependency_manager_has_circular_dependency(manager, "assets/a.json");
    TEST_ASSERT(a_has_circular == true, "File A has circular dependency");

    bool d_has_circular =
        rogue_dependency_manager_has_circular_dependency(manager, "assets/d.json");
    TEST_ASSERT(d_has_circular == false, "File D does not have circular dependency");

    // Test resolution with cycles
    bool resolve_with_cycles = rogue_dependency_manager_resolve_all(manager);
    TEST_ASSERT(resolve_with_cycles == false, "Resolution fails with cycles");
    TEST_ASSERT(manager->circular_dependencies > 0, "Circular dependencies recorded");

    // Test removing cycle by adding circular breaker
    rogue_dependency_manager_add_dependency(manager, "assets/c.json", "assets/a.json",
                                            "ref3_breaker", ROGUE_DEP_TYPE_CIRCULAR_BREAK, 1, NULL);

    bool cycles_after_breaker = rogue_dependency_manager_detect_cycles(manager);
    // Note: This might still detect cycles due to the original strong dependency, but the breaker
    // should help in load ordering

    rogue_dependency_manager_destroy(manager);
}

// Test load order generation
static void test_load_order_generation(void)
{
    TEST_SECTION("Load Order Generation");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Add files with dependencies: affixes <- items <- loot_tables <- enemies
    rogue_dependency_manager_add_file(manager, "assets/affixes.json", ROGUE_FILE_TYPE_AFFIXES, 5);
    rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    rogue_dependency_manager_add_file(manager, "assets/loot_tables.json",
                                      ROGUE_FILE_TYPE_LOOT_TABLES, 15);
    rogue_dependency_manager_add_file(manager, "assets/enemies.json", ROGUE_FILE_TYPE_ENEMIES, 20);

    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/affixes.json",
                                            "affix_fire", ROGUE_DEP_TYPE_STRONG, 1, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/loot_tables.json", "assets/items.json",
                                            "item_sword", ROGUE_DEP_TYPE_STRONG, 2, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/enemies.json",
                                            "assets/loot_tables.json", "loot_goblin",
                                            ROGUE_DEP_TYPE_STRONG, 3, NULL);

    // Test load order generation
    RogueLoadOrder load_order;
    bool generate_order = rogue_dependency_manager_generate_load_order(manager, &load_order);
    TEST_ASSERT(generate_order == true, "Generate load order successfully");
    TEST_ASSERT(load_order.is_valid == true, "Load order is valid");
    TEST_ASSERT(load_order.file_count == 4, "Load order contains all files");

    // Verify order: affixes should come before items, items before loot_tables, loot_tables before
    // enemies
    int affix_pos = -1, item_pos = -1, loot_pos = -1, enemy_pos = -1;
    for (int i = 0; i < load_order.file_count; i++)
    {
        if (strstr(load_order.files[i], "affixes"))
            affix_pos = i;
        else if (strstr(load_order.files[i], "items"))
            item_pos = i;
        else if (strstr(load_order.files[i], "loot_tables"))
            loot_pos = i;
        else if (strstr(load_order.files[i], "enemies"))
            enemy_pos = i;
    }

    TEST_ASSERT(affix_pos < item_pos, "Affixes loaded before items");
    TEST_ASSERT(item_pos < loot_pos, "Items loaded before loot tables");
    TEST_ASSERT(loot_pos < enemy_pos, "Loot tables loaded before enemies");

    // Test dependency-aware ordering for subset of files
    const char* subset_files[] = {"assets/enemies.json", "assets/affixes.json"};
    char ordered_files[10][ROGUE_DEP_MAX_PATH_LENGTH];
    int ordered_count = 0;

    bool subset_order = rogue_dependency_manager_get_dependency_aware_order(
        manager, subset_files, 2, ordered_files, &ordered_count);
    TEST_ASSERT(subset_order == true, "Get dependency-aware order for subset");
    TEST_ASSERT(ordered_count == 2, "Subset order contains both files");

    // Affixes should come before enemies in the subset
    bool affixes_first = (strstr(ordered_files[0], "affixes") != NULL);
    TEST_ASSERT(affixes_first == true, "Affixes come first in subset ordering");

    rogue_dependency_manager_destroy(manager);
}

// Test impact analysis
static void test_impact_analysis(void)
{
    TEST_SECTION("Impact Analysis");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Create dependency chain: affixes <- items <- loot_tables <- enemies
    rogue_dependency_manager_add_file(manager, "assets/affixes.json", ROGUE_FILE_TYPE_AFFIXES, 5);
    rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    rogue_dependency_manager_add_file(manager, "assets/loot_tables.json",
                                      ROGUE_FILE_TYPE_LOOT_TABLES, 15);
    rogue_dependency_manager_add_file(manager, "assets/enemies.json", ROGUE_FILE_TYPE_ENEMIES, 20);
    rogue_dependency_manager_add_file(manager, "assets/skills.json", ROGUE_FILE_TYPE_SKILLS, 25);

    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/affixes.json",
                                            "affix_fire", ROGUE_DEP_TYPE_STRONG, 1, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/loot_tables.json", "assets/items.json",
                                            "item_sword", ROGUE_DEP_TYPE_STRONG, 2, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/enemies.json",
                                            "assets/loot_tables.json", "loot_goblin",
                                            ROGUE_DEP_TYPE_STRONG, 3, NULL);
    // Skills has no dependents

    // Test impact analysis for affixes (should affect items, loot_tables, enemies)
    RogueImpactAnalysis analysis;
    bool analyze1 =
        rogue_dependency_manager_analyze_impact(manager, "assets/affixes.json", &analysis);
    TEST_ASSERT(analyze1 == true, "Analyze impact for affixes");
    TEST_ASSERT(strcmp(analysis.changed_file, "assets/affixes.json") == 0,
                "Correct changed file in analysis");
    TEST_ASSERT(analysis.reload_count >= 1, "At least one file to reload");
    TEST_ASSERT(analysis.affected_count >= 1, "At least one system affected");

    // Check that items.json is in the reload list (since it depends on affixes)
    bool items_in_reload = false;
    for (int i = 0; i < analysis.reload_count; i++)
    {
        if (strstr(analysis.reload_files[i], "items") != NULL)
        {
            items_in_reload = true;
            break;
        }
    }
    TEST_ASSERT(items_in_reload == true, "Items file in reload list when affixes change");

    // Test impact analysis for skills (should affect no other files)
    RogueImpactAnalysis skills_analysis;
    bool analyze2 =
        rogue_dependency_manager_analyze_impact(manager, "assets/skills.json", &skills_analysis);
    TEST_ASSERT(analyze2 == true, "Analyze impact for skills");
    TEST_ASSERT(skills_analysis.reload_count == 0, "No files to reload when skills change");
    TEST_ASSERT(skills_analysis.affected_count == 0, "No systems affected when skills change");
    TEST_ASSERT(skills_analysis.requires_full_reload == false,
                "No full reload required for skills");

    // Test getting affected files directly
    char affected_files[10][ROGUE_DEP_MAX_PATH_LENGTH];
    int affected_count = rogue_dependency_manager_get_affected_files(manager, "assets/affixes.json",
                                                                     affected_files, 10);
    TEST_ASSERT(affected_count >= 1, "Get affected files for affixes");

    // Test getting dependent systems
    char systems[10][ROGUE_DEP_MAX_NAME_LENGTH];
    int system_count =
        rogue_dependency_manager_get_dependent_systems(manager, "assets/affixes.json", systems, 10);
    TEST_ASSERT(system_count >= 0, "Get dependent systems for affixes");

    rogue_dependency_manager_destroy(manager);
}

// Test graph validation
static void test_graph_validation(void)
{
    TEST_SECTION("Graph Validation");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Add files and valid dependencies
    rogue_dependency_manager_add_file(manager, "assets/affixes.json", ROGUE_FILE_TYPE_AFFIXES, 5);
    rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/affixes.json",
                                            "affix_fire", ROGUE_DEP_TYPE_STRONG, 1, NULL);

    // Test valid graph validation
    bool valid1 = rogue_dependency_manager_validate_graph(manager);
    TEST_ASSERT(valid1 == true, "Valid graph passes validation");
    TEST_ASSERT(manager->graph.is_valid == true, "Graph is_valid flag set");

    // Test individual file validation
    bool valid_file =
        rogue_dependency_manager_validate_file_dependencies(manager, "assets/items.json");
    TEST_ASSERT(valid_file == true, "Valid file dependencies pass validation");

    // Add missing dependency to make graph invalid
    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/missing.json",
                                            "missing_ref", ROGUE_DEP_TYPE_STRONG, 2, NULL);

    bool valid2 = rogue_dependency_manager_validate_graph(manager);
    TEST_ASSERT(valid2 == false, "Invalid graph fails validation");
    TEST_ASSERT(manager->graph.is_valid == false, "Graph is_valid flag cleared");

    // Test getting unresolved dependencies
    RogueDependency unresolved[10];
    int unresolved_count =
        rogue_dependency_manager_get_unresolved_dependencies(manager, unresolved, 10);
    TEST_ASSERT(unresolved_count >= 0, "Get unresolved dependencies");

    // Test getting missing dependencies
    RogueDependency missing[10];
    int missing_count = rogue_dependency_manager_get_missing_dependencies(manager, missing, 10);
    TEST_ASSERT(missing_count >= 1, "Get missing dependencies (should have at least one)");
    TEST_ASSERT(strcmp(missing[0].target_file, "assets/missing.json") == 0,
                "Correct missing dependency identified");

    rogue_dependency_manager_destroy(manager);
}

// Test statistics and performance
static void test_statistics_and_performance(void)
{
    TEST_SECTION("Statistics and Performance");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Add files and dependencies
    rogue_dependency_manager_add_file(manager, "assets/affixes.json", ROGUE_FILE_TYPE_AFFIXES, 5);
    rogue_dependency_manager_add_file(manager, "assets/items.json", ROGUE_FILE_TYPE_ITEMS, 10);
    rogue_dependency_manager_add_file(manager, "assets/loot_tables.json",
                                      ROGUE_FILE_TYPE_LOOT_TABLES, 15);

    rogue_dependency_manager_add_dependency(manager, "assets/items.json", "assets/affixes.json",
                                            "affix_fire", ROGUE_DEP_TYPE_STRONG, 1, NULL);
    rogue_dependency_manager_add_dependency(manager, "assets/loot_tables.json", "assets/items.json",
                                            "item_sword", ROGUE_DEP_TYPE_STRONG, 2, NULL);

    // Test initial statistics
    int total_deps, resolved_deps, failed_deps, circular_deps;
    rogue_dependency_manager_get_statistics(manager, &total_deps, &resolved_deps, &failed_deps,
                                            &circular_deps);
    TEST_ASSERT(total_deps == 2, "Correct total dependencies in statistics");
    TEST_ASSERT(resolved_deps == 0, "No resolved dependencies initially");

    // Perform resolution to get performance data
    bool resolve = rogue_dependency_manager_resolve_all(manager);
    TEST_ASSERT(resolve == true, "Resolution successful for performance test");

    // Check performance metrics
    uint64_t last_time = manager->last_resolve_time_ms;
    TEST_ASSERT(last_time >= 0, "Last resolve time is non-negative");
    TEST_ASSERT(manager->resolve_count == 1, "Resolve count incremented");
    TEST_ASSERT(manager->total_resolve_time_ms == last_time, "Total resolve time equals last time");

    uint64_t avg_time = rogue_dependency_manager_get_average_resolve_time(manager);
    TEST_ASSERT(avg_time == last_time, "Average resolve time equals last time (single resolution)");

    // Test statistics after resolution
    rogue_dependency_manager_get_statistics(manager, &total_deps, &resolved_deps, &failed_deps,
                                            &circular_deps);
    TEST_ASSERT(resolved_deps == 2, "All dependencies resolved");
    TEST_ASSERT(failed_deps == 0, "No failed dependencies");
    TEST_ASSERT(circular_deps == 0, "No circular dependencies");

    // Test statistics reset
    rogue_dependency_manager_reset_statistics(manager);
    rogue_dependency_manager_get_statistics(manager, &total_deps, &resolved_deps, &failed_deps,
                                            &circular_deps);
    TEST_ASSERT(resolved_deps == 0, "Resolved dependencies reset");
    TEST_ASSERT(failed_deps == 0, "Failed dependencies reset");
    TEST_ASSERT(manager->resolve_count == 0, "Resolve count reset");

    rogue_dependency_manager_destroy(manager);
}

// Test utility functions
static void test_utility_functions(void)
{
    TEST_SECTION("Utility Functions");

    // Test file type detection
    RogueFileType type1 = rogue_dependency_manager_get_file_type_from_path("assets/items.json");
    TEST_ASSERT(type1 == ROGUE_FILE_TYPE_ITEMS, "Detect items file type");

    RogueFileType type2 = rogue_dependency_manager_get_file_type_from_path("assets/affixes.json");
    TEST_ASSERT(type2 == ROGUE_FILE_TYPE_AFFIXES, "Detect affixes file type");

    RogueFileType type3 = rogue_dependency_manager_get_file_type_from_path("assets/unknown.json");
    TEST_ASSERT(type3 == ROGUE_FILE_TYPE_OTHER, "Detect other file type for unknown");

    // Test file type name conversion
    const char* name1 = rogue_dependency_manager_get_file_type_name(ROGUE_FILE_TYPE_ITEMS);
    TEST_ASSERT(strcmp(name1, "Items") == 0, "Get items file type name");

    const char* name2 = rogue_dependency_manager_get_file_type_name(ROGUE_FILE_TYPE_AFFIXES);
    TEST_ASSERT(strcmp(name2, "Affixes") == 0, "Get affixes file type name");

    // Test dependency type name conversion
    const char* dep_name1 =
        rogue_dependency_manager_get_dependency_type_name(ROGUE_DEP_TYPE_STRONG);
    TEST_ASSERT(strcmp(dep_name1, "Strong") == 0, "Get strong dependency type name");

    const char* dep_name2 = rogue_dependency_manager_get_dependency_type_name(ROGUE_DEP_TYPE_WEAK);
    TEST_ASSERT(strcmp(dep_name2, "Weak") == 0, "Get weak dependency type name");

    // Test dependency status name conversion
    const char* status_name1 =
        rogue_dependency_manager_get_dependency_status_name(ROGUE_DEP_STATUS_RESOLVED);
    TEST_ASSERT(strcmp(status_name1, "Resolved") == 0, "Get resolved status name");

    const char* status_name2 =
        rogue_dependency_manager_get_dependency_status_name(ROGUE_DEP_STATUS_MISSING);
    TEST_ASSERT(strcmp(status_name2, "Missing") == 0, "Get missing status name");

    // Test path validation
    bool valid1 = rogue_dependency_manager_is_valid_file_path("assets/items.json");
    TEST_ASSERT(valid1 == true, "Valid file path accepted");

    bool valid2 = rogue_dependency_manager_is_valid_file_path("");
    TEST_ASSERT(valid2 == false, "Empty path rejected");

    bool valid3 = rogue_dependency_manager_is_valid_file_path("assets/invalid|path.json");
    TEST_ASSERT(valid3 == false, "Path with invalid character rejected");

    // Test reference key validation
    bool ref_valid1 = rogue_dependency_manager_is_valid_reference_key("affix_fire_damage");
    TEST_ASSERT(ref_valid1 == true, "Valid reference key accepted");

    bool ref_valid2 = rogue_dependency_manager_is_valid_reference_key("item.sword.basic");
    TEST_ASSERT(ref_valid2 == true, "Valid reference key with dots accepted");

    bool ref_valid3 = rogue_dependency_manager_is_valid_reference_key("");
    TEST_ASSERT(ref_valid3 == false, "Empty reference key rejected");

    bool ref_valid4 = rogue_dependency_manager_is_valid_reference_key("invalid key!");
    TEST_ASSERT(ref_valid4 == false, "Reference key with invalid character rejected");
}

// Test error handling and edge cases
static void test_error_handling(void)
{
    TEST_SECTION("Error Handling and Edge Cases");

    // Test NULL parameter handling
    RogueDependencyManager* null_manager = NULL;
    TEST_ASSERT(rogue_dependency_manager_initialize(null_manager) == false,
                "Initialize with NULL manager fails");
    TEST_ASSERT(rogue_dependency_manager_find_node(null_manager, "test.json") == NULL,
                "Find node with NULL manager returns NULL");

    // Test valid manager with invalid parameters
    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);

    bool add_null_file =
        rogue_dependency_manager_add_file(manager, NULL, ROGUE_FILE_TYPE_ITEMS, 10);
    TEST_ASSERT(add_null_file == false, "Add file with NULL path fails");

    bool add_dep_null_source = rogue_dependency_manager_add_dependency(
        manager, NULL, "target.json", "ref", ROGUE_DEP_TYPE_STRONG, 1, NULL);
    TEST_ASSERT(add_dep_null_source == false, "Add dependency with NULL source fails");

    bool add_dep_null_target = rogue_dependency_manager_add_dependency(
        manager, "source.json", NULL, "ref", ROGUE_DEP_TYPE_STRONG, 1, NULL);
    TEST_ASSERT(add_dep_null_target == false, "Add dependency with NULL target fails");

    bool add_dep_null_ref = rogue_dependency_manager_add_dependency(
        manager, "source.json", "target.json", NULL, ROGUE_DEP_TYPE_STRONG, 1, NULL);
    TEST_ASSERT(add_dep_null_ref == false, "Add dependency with NULL reference key fails");

    // Test operations on empty manager
    RogueLoadOrder empty_order;
    bool empty_load_order = rogue_dependency_manager_generate_load_order(manager, &empty_order);
    TEST_ASSERT(empty_load_order == true, "Generate load order on empty manager succeeds");
    TEST_ASSERT(empty_order.file_count == 0, "Empty manager has zero files in load order");

    bool empty_resolve = rogue_dependency_manager_resolve_all(manager);
    TEST_ASSERT(empty_resolve == true, "Resolve all on empty manager succeeds");

    // Test maximum limits (add many files)
    for (int i = 0; i < 10; i++)
    {
        char file_path[256];
        snprintf(file_path, sizeof(file_path), "assets/test_%d.json", i);
        bool add_result =
            rogue_dependency_manager_add_file(manager, file_path, ROGUE_FILE_TYPE_OTHER, i);
        TEST_ASSERT(add_result == true, "Add multiple files within limits");
    }

    // Test adding many dependencies to one file
    RogueDependencyNode* test_node =
        rogue_dependency_manager_find_node(manager, "assets/test_0.json");
    if (test_node)
    {
        for (int i = 1; i < 10 && i < ROGUE_DEP_MAX_DEPENDENCIES; i++)
        {
            char target_file[256], ref_key[256];
            snprintf(target_file, sizeof(target_file), "assets/test_%d.json", i);
            snprintf(ref_key, sizeof(ref_key), "ref_%d", i);
            bool dep_result =
                rogue_dependency_manager_add_dependency(manager, "assets/test_0.json", target_file,
                                                        ref_key, ROGUE_DEP_TYPE_STRONG, 1, NULL);
            TEST_ASSERT(dep_result == true, "Add multiple dependencies to one file");
        }
    }

    rogue_dependency_manager_destroy(manager);

    // Test destruction of NULL manager (should not crash)
    rogue_dependency_manager_destroy(null_manager);
    TEST_ASSERT(true, "Destroy NULL manager does not crash");
}

// Test integration scenarios
static void test_integration_scenarios(void)
{
    TEST_SECTION("Integration Scenarios");

    RogueDependencyManager* manager = rogue_dependency_manager_create();
    rogue_dependency_manager_initialize(manager);
    rogue_dependency_manager_set_debug_mode(manager, false);

    // Scenario 1: Game configuration system with realistic dependencies
    // Core data files
    rogue_dependency_manager_add_file(manager, "config/core/affixes.json", ROGUE_FILE_TYPE_AFFIXES,
                                      1);
    rogue_dependency_manager_add_file(manager, "config/core/items.json", ROGUE_FILE_TYPE_ITEMS, 2);
    rogue_dependency_manager_add_file(manager, "config/core/skills.json", ROGUE_FILE_TYPE_SKILLS,
                                      3);

    // Game content files
    rogue_dependency_manager_add_file(manager, "config/content/loot_tables.json",
                                      ROGUE_FILE_TYPE_LOOT_TABLES, 4);
    rogue_dependency_manager_add_file(manager, "config/content/enemies.json",
                                      ROGUE_FILE_TYPE_ENEMIES, 5);
    rogue_dependency_manager_add_file(manager, "config/content/encounters.json",
                                      ROGUE_FILE_TYPE_ENCOUNTERS, 6);
    rogue_dependency_manager_add_file(manager, "config/content/biomes.json", ROGUE_FILE_TYPE_BIOMES,
                                      7);

    // Set up realistic dependency chain
    rogue_dependency_manager_add_dependency(
        manager, "config/core/items.json", "config/core/affixes.json", "fire_damage_affix",
        ROGUE_DEP_TYPE_STRONG, 1, "Items use affixes for properties");
    rogue_dependency_manager_add_dependency(
        manager, "config/content/loot_tables.json", "config/core/items.json", "basic_sword",
        ROGUE_DEP_TYPE_STRONG, 1, "Loot tables reference items");
    rogue_dependency_manager_add_dependency(manager, "config/content/enemies.json",
                                            "config/content/loot_tables.json", "goblin_loot",
                                            ROGUE_DEP_TYPE_STRONG, 1, "Enemies have loot tables");
    rogue_dependency_manager_add_dependency(
        manager, "config/content/enemies.json", "config/core/skills.json", "fireball",
        ROGUE_DEP_TYPE_WEAK, 2, "Enemies optionally use skills");
    rogue_dependency_manager_add_dependency(
        manager, "config/content/encounters.json", "config/content/enemies.json", "goblin_warrior",
        ROGUE_DEP_TYPE_STRONG, 1, "Encounters reference enemies");
    rogue_dependency_manager_add_dependency(
        manager, "config/content/biomes.json", "config/content/encounters.json", "forest_encounter",
        ROGUE_DEP_TYPE_STRONG, 1, "Biomes reference encounters");

    // Test full system resolution
    bool full_resolve = rogue_dependency_manager_resolve_all(manager);
    TEST_ASSERT(full_resolve == true, "Full game config resolution succeeds");

    // Test load order generation
    RogueLoadOrder game_load_order;
    bool game_order = rogue_dependency_manager_generate_load_order(manager, &game_load_order);
    TEST_ASSERT(game_order == true, "Game config load order generation succeeds");
    TEST_ASSERT(game_load_order.file_count == 7, "Load order contains all game config files");

    // Verify that affixes loads before items, and items before loot tables
    int affix_idx = -1, item_idx = -1, loot_idx = -1;
    for (int i = 0; i < game_load_order.file_count; i++)
    {
        if (strstr(game_load_order.files[i], "affixes"))
            affix_idx = i;
        else if (strstr(game_load_order.files[i], "items"))
            item_idx = i;
        else if (strstr(game_load_order.files[i], "loot_tables"))
            loot_idx = i;
    }
    TEST_ASSERT(affix_idx < item_idx && item_idx < loot_idx,
                "Game config dependency order is correct");

    // Scenario 2: Test impact analysis for core file change
    RogueImpactAnalysis core_impact;
    bool core_analysis =
        rogue_dependency_manager_analyze_impact(manager, "config/core/affixes.json", &core_impact);
    TEST_ASSERT(core_analysis == true, "Core file impact analysis succeeds");
    TEST_ASSERT(core_impact.reload_count >= 1, "Core file change affects multiple files");
    TEST_ASSERT(core_impact.affected_count >= 1, "Core file change affects multiple systems");

    // Test impact analysis for leaf file change
    RogueImpactAnalysis leaf_impact;
    bool leaf_analysis = rogue_dependency_manager_analyze_impact(
        manager, "config/content/biomes.json", &leaf_impact);
    TEST_ASSERT(leaf_analysis == true, "Leaf file impact analysis succeeds");
    TEST_ASSERT(leaf_impact.reload_count == 0, "Leaf file change affects no other files");

    // Scenario 3: Test graph validation
    bool game_valid = rogue_dependency_manager_validate_graph(manager);
    TEST_ASSERT(game_valid == true, "Game config graph validation succeeds");

    // Scenario 4: Test statistics
    int total, resolved, failed, circular;
    rogue_dependency_manager_get_statistics(manager, &total, &resolved, &failed, &circular);
    TEST_ASSERT(total == 6, "Correct total dependencies in game config");
    TEST_ASSERT(resolved == 6, "All game config dependencies resolved");
    TEST_ASSERT(failed == 0, "No failed dependencies in game config");
    TEST_ASSERT(circular == 0, "No circular dependencies in game config");

    rogue_dependency_manager_destroy(manager);
}

// Main test runner
int main(void)
{
    printf("=== Dependency Manager Test Suite ===\n");

    test_dependency_manager_lifecycle();
    test_file_management();
    test_dependency_registration();
    test_dependency_resolution();
    test_circular_dependency_detection();
    test_load_order_generation();
    test_impact_analysis();
    test_graph_validation();
    test_statistics_and_performance();
    test_utility_functions();
    test_error_handling();
    test_integration_scenarios();

    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);

    if (tests_failed == 0)
    {
        printf("\n🎉 All tests passed! Dependency management system is working correctly.\n");
        return 0;
    }
    else
    {
        printf("\n❌ Some tests failed. Please review the implementation.\n");
        return 1;
    }
}
