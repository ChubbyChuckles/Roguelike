/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "core/integration/system_taxonomy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* External reference to global taxonomy for testing */
extern RogueSystemTaxonomy g_system_taxonomy;

/* Test framework utilities */
static int test_count = 0;
static int test_passed = 0;

static void run_test(const char* test_name, bool (*test_func)(void))
{
    test_count++;
    printf("Running test: %s...\n", test_name);

    /* Reset taxonomy for each test */
    rogue_system_taxonomy_shutdown();

    if (test_func())
    {
        printf("  PASS\n");
        test_passed++;
    }
    else
    {
        printf("  FAIL\n");
    }
}

/* Test 1: Taxonomy initialization and shutdown */
static bool test_taxonomy_initialization(void)
{
    /* Test successful initialization */
    bool init_result = rogue_system_taxonomy_init();
    if (!init_result)
    {
        printf("    ERROR: Taxonomy initialization failed\n");
        return false;
    }

    if (!g_system_taxonomy.initialized)
    {
        printf("    ERROR: Taxonomy not marked as initialized\n");
        return false;
    }

    /* Should have known systems populated */
    if (g_system_taxonomy.system_count == 0)
    {
        printf("    ERROR: No systems populated after initialization\n");
        return false;
    }

    printf("    INFO: Initialized with %u systems\n", g_system_taxonomy.system_count);

    /* Test shutdown */
    rogue_system_taxonomy_shutdown();

    if (g_system_taxonomy.initialized || g_system_taxonomy.system_count != 0)
    {
        printf("    ERROR: Taxonomy not properly reset after shutdown\n");
        return false;
    }

    return true;
}

/* Test 2: System addition and retrieval */
static bool test_system_addition(void)
{
    rogue_system_taxonomy_init();
    uint32_t initial_count = g_system_taxonomy.system_count;

    /* Create test system */
    RogueSystemInfo test_system = {.system_id = 9999,
                                   .name = "Test System",
                                   .description = "System created for unit testing",
                                   .type = ROGUE_SYSTEM_TYPE_CORE,
                                   .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT,
                                   .capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE |
                                                   ROGUE_SYSTEM_CAP_CONFIGURABLE,
                                   .is_implemented = false,
                                   .implementation_status = "Test",
                                   .version = "1.0"};

    /* Test successful addition */
    bool add_result = rogue_system_taxonomy_add_system(&test_system);
    if (!add_result)
    {
        printf("    ERROR: Failed to add test system\n");
        return false;
    }

    if (g_system_taxonomy.system_count != initial_count + 1)
    {
        printf("    ERROR: System count not incremented after addition\n");
        return false;
    }

    /* Test retrieval by ID */
    const RogueSystemInfo* retrieved = rogue_system_taxonomy_get_system(9999);
    if (!retrieved)
    {
        printf("    ERROR: Could not retrieve system by ID\n");
        return false;
    }

    if (strcmp(retrieved->name, "Test System") != 0)
    {
        printf("    ERROR: Retrieved system has wrong name\n");
        return false;
    }

    /* Test retrieval by name */
    const RogueSystemInfo* found_by_name = rogue_system_taxonomy_find_system_by_name("Test System");
    if (!found_by_name)
    {
        printf("    ERROR: Could not find system by name\n");
        return false;
    }

    if (found_by_name->system_id != 9999)
    {
        printf("    ERROR: Found system has wrong ID\n");
        return false;
    }

    /* Test duplicate name rejection */
    RogueSystemInfo duplicate_system = test_system;
    duplicate_system.system_id = 8888;

    bool duplicate_result = rogue_system_taxonomy_add_system(&duplicate_system);
    if (duplicate_result)
    {
        printf("    ERROR: Duplicate system name was accepted\n");
        return false;
    }

    return true;
}

/* Test 3: System classification and counting */
static bool test_system_classification(void)
{
    rogue_system_taxonomy_init();

    /* Test counting by type */
    uint32_t core_count = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_CORE);
    uint32_t content_count = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_CONTENT);
    uint32_t ui_count = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_UI);
    uint32_t infra_count = rogue_system_taxonomy_count_by_type(ROGUE_SYSTEM_TYPE_INFRASTRUCTURE);

    printf("    INFO: Core: %u, Content: %u, UI: %u, Infrastructure: %u\n", core_count,
           content_count, ui_count, infra_count);

    /* Verify counts add up to total */
    uint32_t total_by_type = core_count + content_count + ui_count + infra_count;
    if (total_by_type != g_system_taxonomy.system_count)
    {
        printf("    ERROR: Type counts don't add up to total\n");
        return false;
    }

    /* Test counting by priority */
    uint32_t critical_count =
        rogue_system_taxonomy_count_by_priority(ROGUE_SYSTEM_PRIORITY_CRITICAL);
    uint32_t important_count =
        rogue_system_taxonomy_count_by_priority(ROGUE_SYSTEM_PRIORITY_IMPORTANT);
    uint32_t optional_count =
        rogue_system_taxonomy_count_by_priority(ROGUE_SYSTEM_PRIORITY_OPTIONAL);

    printf("    INFO: Critical: %u, Important: %u, Optional: %u\n", critical_count, important_count,
           optional_count);

    /* Test implemented count */
    uint32_t implemented_count = rogue_system_taxonomy_count_implemented();
    printf("    INFO: Implemented: %u of %u systems\n", implemented_count,
           g_system_taxonomy.system_count);

    if (implemented_count > g_system_taxonomy.system_count)
    {
        printf("    ERROR: Implemented count exceeds total count\n");
        return false;
    }

    /* Test capability counting */
    uint32_t update_capable =
        rogue_system_taxonomy_count_by_capability(ROGUE_SYSTEM_CAP_REQUIRES_UPDATE);
    uint32_t event_producers =
        rogue_system_taxonomy_count_by_capability(ROGUE_SYSTEM_CAP_PRODUCES_EVENTS);

    printf("    INFO: Update-capable: %u, Event producers: %u\n", update_capable, event_producers);

    return true;
}

/* Test 4: Capability matrix generation */
static bool test_capability_matrix(void)
{
    rogue_system_taxonomy_init();

    char matrix_buffer[2048];
    printf("    About to generate capability matrix...\n");
    rogue_system_taxonomy_generate_capability_matrix(matrix_buffer, sizeof(matrix_buffer));
    printf("    Capability matrix generation completed\n");

    if (strlen(matrix_buffer) == 0)
    {
        printf("    ERROR: Capability matrix generation failed\n");
        return false;
    }

    /* Check for expected content */
    if (!strstr(matrix_buffer, "System Capability Matrix"))
    {
        printf("    ERROR: Matrix missing header\n");
        return false;
    }

    if (!strstr(matrix_buffer, "systems"))
    {
        printf("    ERROR: Matrix missing system counts\n");
        return false;
    }

    printf("    INFO: Generated capability matrix (%zu bytes)\n", strlen(matrix_buffer));

    /* Test with null buffer (should not crash) */
    printf("    Testing null buffer handling...\n");
    rogue_system_taxonomy_generate_capability_matrix(NULL, 0);
    printf("    Null buffer test completed\n");

    /* Test with small buffer */
    printf("    Testing small buffer handling...\n");
    char small_buffer[10];
    rogue_system_taxonomy_generate_capability_matrix(small_buffer, sizeof(small_buffer));
    printf("    Small buffer test completed\n");

    return true;
}

/* Test 5: Resource usage analysis */
static bool test_resource_analysis(void)
{
    rogue_system_taxonomy_init();

    char analysis_buffer[2048];
    rogue_system_taxonomy_analyze_resource_usage(analysis_buffer, sizeof(analysis_buffer));

    if (strlen(analysis_buffer) == 0)
    {
        printf("    ERROR: Resource analysis generation failed\n");
        return false;
    }

    /* Check for expected content */
    if (!strstr(analysis_buffer, "Resource Usage Analysis"))
    {
        printf("    ERROR: Analysis missing header\n");
        return false;
    }

    if (!strstr(analysis_buffer, "Total Systems"))
    {
        printf("    ERROR: Analysis missing system count\n");
        return false;
    }

    printf("    INFO: Generated resource analysis (%zu bytes)\n", strlen(analysis_buffer));

    return true;
}

/* Test 6: Initialization report generation */
static bool test_init_report(void)
{
    rogue_system_taxonomy_init();

    char report_buffer[4096];
    rogue_system_taxonomy_generate_init_report(report_buffer, sizeof(report_buffer));

    if (strlen(report_buffer) == 0)
    {
        printf("    ERROR: Initialization report generation failed\n");
        return false;
    }

    /* Check for expected content */
    if (!strstr(report_buffer, "Initialization Requirements"))
    {
        printf("    ERROR: Report missing header\n");
        return false;
    }

    /* Should contain priority sections */
    if (!strstr(report_buffer, "Priority Systems"))
    {
        printf("    ERROR: Report missing priority sections\n");
        return false;
    }

    printf("    INFO: Generated initialization report (%zu bytes)\n", strlen(report_buffer));

    return true;
}

/* Test 7: Known systems population validation */
static bool test_known_systems(void)
{
    rogue_system_taxonomy_init();

    /* Verify specific known systems exist */
    const char* expected_systems[] = {
        "AI System",    "Combat System",       "Character Progression",
        "Skill System", "Loot & Item System",  "Equipment System",
        "UI System",    "Integration Plumbing"};

    int expected_count = sizeof(expected_systems) / sizeof(expected_systems[0]);

    for (int i = 0; i < expected_count; i++)
    {
        const RogueSystemInfo* system =
            rogue_system_taxonomy_find_system_by_name(expected_systems[i]);
        if (!system)
        {
            printf("    ERROR: Expected system '%s' not found\n", expected_systems[i]);
            return false;
        }

        /* Validate system has proper data */
        if (!system->description || strlen(system->description) == 0)
        {
            printf("    ERROR: System '%s' missing description\n", expected_systems[i]);
            return false;
        }

        if (system->type >= ROGUE_SYSTEM_TYPE_COUNT)
        {
            printf("    ERROR: System '%s' has invalid type\n", expected_systems[i]);
            return false;
        }
    }

    printf("    INFO: Validated %d expected systems\n", expected_count);

    /* Verify we have reasonable number of systems */
    if (g_system_taxonomy.system_count < 10)
    {
        printf("    ERROR: Too few systems populated (%u)\n", g_system_taxonomy.system_count);
        return false;
    }

    if (g_system_taxonomy.system_count > 50)
    {
        printf("    ERROR: Too many systems populated (%u)\n", g_system_taxonomy.system_count);
        return false;
    }

    return true;
}

/* Test 8: Taxonomy validation */
static bool test_taxonomy_validation(void)
{
    rogue_system_taxonomy_init();

    /* Should pass validation initially */
    bool validation_result = rogue_system_taxonomy_validate();
    if (!validation_result)
    {
        printf("    ERROR: Taxonomy failed validation after initialization\n");
        return false;
    }

    /* Test validation with uninitialized taxonomy */
    rogue_system_taxonomy_shutdown();
    validation_result = rogue_system_taxonomy_validate();
    if (validation_result)
    {
        printf("    ERROR: Validation passed on uninitialized taxonomy\n");
        return false;
    }

    /* Re-initialize and add invalid system to test validation */
    rogue_system_taxonomy_init();

    /* Directly corrupt taxonomy for testing (don't use this pattern in real code) */
    if (g_system_taxonomy.system_count > 0)
    {
        /* Save original and corrupt */
        RogueSystemType original_type = g_system_taxonomy.systems[0].type;
        g_system_taxonomy.systems[0].type = (RogueSystemType) 999; /* Invalid type */

        validation_result = rogue_system_taxonomy_validate();
        if (validation_result)
        {
            printf("    ERROR: Validation passed with corrupted system type\n");
            return false;
        }

        /* Restore original */
        g_system_taxonomy.systems[0].type = original_type;

        /* Should pass again */
        validation_result = rogue_system_taxonomy_validate();
        if (!validation_result)
        {
            printf("    ERROR: Validation failed after restoring valid data\n");
            return false;
        }
    }

    return true;
}

/* Test 9: Comprehensive report generation */
static bool test_comprehensive_report(void)
{
    rogue_system_taxonomy_init();

    char report_buffer[8192];
    rogue_system_taxonomy_generate_report(report_buffer, sizeof(report_buffer));

    if (strlen(report_buffer) == 0)
    {
        printf("    ERROR: Comprehensive report generation failed\n");
        return false;
    }

    /* Check for expected sections */
    if (!strstr(report_buffer, "System Taxonomy Report"))
    {
        printf("    ERROR: Report missing main header\n");
        return false;
    }

    if (!strstr(report_buffer, "Summary Statistics"))
    {
        printf("    ERROR: Report missing summary statistics\n");
        return false;
    }

    if (!strstr(report_buffer, "Implementation Status"))
    {
        printf("    ERROR: Report missing implementation status\n");
        return false;
    }

    if (!strstr(report_buffer, "Total Systems"))
    {
        printf("    ERROR: Report missing system count\n");
        return false;
    }

    printf("    INFO: Generated comprehensive report (%zu bytes)\n", strlen(report_buffer));

    /* Test with small buffer to ensure no buffer overflows */
    char small_report[100];
    rogue_system_taxonomy_generate_report(small_report, sizeof(small_report));

    return true;
}

/* Test 10: Edge cases and error handling */
static bool test_edge_cases(void)
{
    rogue_system_taxonomy_init();

    /* Test null parameter handling */
    bool result = rogue_system_taxonomy_add_system(NULL);
    if (result)
    {
        printf("    ERROR: NULL system addition was accepted\n");
        return false;
    }

    const RogueSystemInfo* null_search = rogue_system_taxonomy_find_system_by_name(NULL);
    if (null_search)
    {
        printf("    ERROR: NULL name search returned a system\n");
        return false;
    }

    const RogueSystemInfo* not_found = rogue_system_taxonomy_get_system(99999);
    if (not_found)
    {
        printf("    ERROR: Invalid system ID returned a system\n");
        return false;
    }

    /* Test with uninitialized taxonomy */
    rogue_system_taxonomy_shutdown();

    RogueSystemInfo test_system = {.system_id = 1000,
                                   .name = "Test",
                                   .type = ROGUE_SYSTEM_TYPE_CORE,
                                   .priority = ROGUE_SYSTEM_PRIORITY_IMPORTANT};

    result = rogue_system_taxonomy_add_system(&test_system);
    if (result)
    {
        printf("    ERROR: System addition succeeded on uninitialized taxonomy\n");
        return false;
    }

    /* Test count functions with uninitialized taxonomy */
    uint32_t count = rogue_system_taxonomy_get_system_count();
    if (count != 0)
    {
        printf("    ERROR: Uninitialized taxonomy reported non-zero count\n");
        return false;
    }

    return true;
}

/* Main test execution */
int main(void)
{
    printf("=== System Taxonomy Unit Tests ===\n\n");

    /* Run all tests */
    run_test("Taxonomy Initialization", test_taxonomy_initialization);
    run_test("System Addition", test_system_addition);
    run_test("System Classification", test_system_classification);
    run_test("Capability Matrix", test_capability_matrix);
    run_test("Resource Analysis", test_resource_analysis);
    run_test("Initialization Report", test_init_report);
    run_test("Known Systems", test_known_systems);
    run_test("Taxonomy Validation", test_taxonomy_validation);
    run_test("Comprehensive Report", test_comprehensive_report);
    run_test("Edge Cases", test_edge_cases);

    /* Summary */
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", test_count);
    printf("Tests passed: %d\n", test_passed);
    printf("Tests failed: %d\n", test_count - test_passed);

    if (test_passed == test_count)
    {
        printf("All tests PASSED!\n");
        return 0;
    }
    else
    {
        printf("Some tests FAILED!\n");
        return 1;
    }
}
