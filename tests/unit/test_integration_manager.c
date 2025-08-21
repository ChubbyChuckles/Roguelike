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

#define SDL_MAIN_HANDLED 1
#include "core/integration/integration_manager.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Test system data structures */
typedef struct
{
    int counter;
    bool initialized;
    bool shutdown_called;
} TestSystemData;

static TestSystemData test_system_data[4] = {{0}};

/* Test system interface implementations */
static bool test_system_init(void* data)
{
    TestSystemData* tsd = (TestSystemData*) data;
    if (!tsd)
        return false;

    tsd->initialized = true;
    tsd->counter = 0;
    tsd->shutdown_called = false;
    return true;
}

static void test_system_update(void* data, double dt_ms)
{
    TestSystemData* tsd = (TestSystemData*) data;
    if (tsd && tsd->initialized)
    {
        tsd->counter++;
    }
}

static void test_system_shutdown(void* data)
{
    TestSystemData* tsd = (TestSystemData*) data;
    if (tsd)
    {
        tsd->shutdown_called = true;
        tsd->initialized = false;
    }
}

static void* test_system_get_state(void* data) { return data; }

/* Test system that always fails initialization */
static bool failing_system_init(void* data) { return false; /* Always fail */ }

static void failing_system_update(void* data, double dt_ms) { /* No-op */ }

static void failing_system_shutdown(void* data) { /* No-op */ }

static void* failing_system_get_state(void* data) { return data; }

/* Helper to create test system descriptor */
static RogueSystemDescriptor create_test_system(const char* name, RogueSystemType type,
                                                RogueSystemPriority priority, TestSystemData* data)
{
    RogueSystemDescriptor desc = {0};
    desc.name = name;
    desc.version = "1.0.0";
    desc.type = type;
    desc.priority = priority;
    desc.capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_CONFIGURABLE;
    desc.resources.cpu_usage_percent = 5;
    desc.resources.memory_usage_kb = 1024;

    desc.interface.init = test_system_init;
    desc.interface.update = test_system_update;
    desc.interface.shutdown = test_system_shutdown;
    desc.interface.get_state = test_system_get_state;

    desc.system_data = data;
    return desc;
}

/* Test functions */
static bool test_manager_initialization(void)
{
    printf("Testing integration manager initialization...\n");

    bool result = rogue_integration_manager_init();
    assert(result == true);
    assert(g_integration_manager.system_count == 0);
    assert(g_integration_manager.next_system_id == 1);
    assert(g_integration_manager.initialization_complete == false);

    rogue_integration_manager_shutdown();
    printf("  ✓ Manager initialization passed\n");
    return true;
}

static bool test_system_registration(void)
{
    printf("Testing system registration...\n");

    rogue_integration_manager_init();

    /* Test valid system registration */
    RogueSystemDescriptor desc1 =
        create_test_system("TestSystem1", ROGUE_SYSTEM_TYPE_CORE, ROGUE_SYSTEM_PRIORITY_CRITICAL,
                           &test_system_data[0]);
    uint32_t system_id1 = rogue_integration_register_system(&desc1);
    assert(system_id1 != 0);
    assert(g_integration_manager.system_count == 1);

    /* Test duplicate name rejection */
    RogueSystemDescriptor desc2 = create_test_system(
        "TestSystem1", ROGUE_SYSTEM_TYPE_UI, ROGUE_SYSTEM_PRIORITY_OPTIONAL, &test_system_data[1]);
    uint32_t system_id2 = rogue_integration_register_system(&desc2);
    assert(system_id2 == 0); /* Should fail due to duplicate name */
    assert(g_integration_manager.system_count == 1);

    /* Test system lookup */
    RogueSystemEntry* entry = rogue_integration_get_system(system_id1);
    assert(entry != NULL);
    assert(strcmp(entry->descriptor.name, "TestSystem1") == 0);
    assert(entry->descriptor.type == ROGUE_SYSTEM_TYPE_CORE);
    assert(entry->descriptor.priority == ROGUE_SYSTEM_PRIORITY_CRITICAL);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_UNINITIALIZED);

    /* Test system lookup by name */
    RogueSystemEntry* entry2 = rogue_integration_find_system_by_name("TestSystem1");
    assert(entry2 == entry);

    /* Test invalid system lookup */
    RogueSystemEntry* entry3 = rogue_integration_get_system(999);
    assert(entry3 == NULL);

    rogue_integration_manager_shutdown();
    printf("  ✓ System registration passed\n");
    return true;
}

static bool test_system_lifecycle(void)
{
    printf("Testing system lifecycle management...\n");

    rogue_integration_manager_init();

    RogueSystemDescriptor desc =
        create_test_system("LifecycleTest", ROGUE_SYSTEM_TYPE_CONTENT,
                           ROGUE_SYSTEM_PRIORITY_IMPORTANT, &test_system_data[0]);
    uint32_t system_id = rogue_integration_register_system(&desc);
    assert(system_id != 0);

    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_UNINITIALIZED);

    /* Test initialization */
    bool init_result = rogue_integration_initialize_system(system_id);
    assert(init_result == true);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_RUNNING);
    assert(test_system_data[0].initialized == true);

    /* Test pause/resume */
    bool pause_result = rogue_integration_pause_system(system_id);
    assert(pause_result == true);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_PAUSED);

    bool resume_result = rogue_integration_resume_system(system_id);
    assert(resume_result == true);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_RUNNING);

    /* Test update calls */
    int initial_counter = test_system_data[0].counter;
    rogue_integration_manager_update(16.67); /* Simulate 60 FPS frame */
    assert(test_system_data[0].counter == initial_counter + 1);

    /* Test shutdown */
    bool shutdown_result = rogue_integration_shutdown_system(system_id);
    assert(shutdown_result == true);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_SHUTDOWN);
    assert(test_system_data[0].shutdown_called == true);

    rogue_integration_manager_shutdown();
    printf("  ✓ System lifecycle passed\n");
    return true;
}

static bool test_failed_system_handling(void)
{
    printf("Testing failed system handling...\n");

    rogue_integration_manager_init();

    /* Create a system that will fail initialization */
    RogueSystemDescriptor desc = {0};
    desc.name = "FailingSystem";
    desc.version = "1.0.0";
    desc.type = ROGUE_SYSTEM_TYPE_INFRASTRUCTURE;
    desc.priority = ROGUE_SYSTEM_PRIORITY_OPTIONAL;
    desc.interface.init = failing_system_init;
    desc.interface.update = failing_system_update;
    desc.interface.shutdown = failing_system_shutdown;
    desc.interface.get_state = failing_system_get_state;
    desc.system_data = &test_system_data[2];

    uint32_t system_id = rogue_integration_register_system(&desc);
    assert(system_id != 0);

    /* Test failed initialization */
    bool init_result = rogue_integration_initialize_system(system_id);
    assert(init_result == false);

    RogueSystemEntry* entry = rogue_integration_get_system(system_id);
    assert(entry->current_state == ROGUE_SYSTEM_STATE_FAILED);
    assert(entry->health.error_count > 0);

    /* Test restart attempts */
    bool restart_result = rogue_integration_restart_system(system_id);
    assert(restart_result == false); /* Should fail again */
    assert(entry->current_state == ROGUE_SYSTEM_STATE_FAILED);

    rogue_integration_manager_shutdown();
    printf("  ✓ Failed system handling passed\n");
    return true;
}

static bool test_dependency_management(void)
{
    printf("Testing dependency management...\n");

    rogue_integration_manager_init();

    /* Create systems with dependencies */
    RogueSystemDescriptor base_desc = create_test_system(
        "BaseSystem", ROGUE_SYSTEM_TYPE_CORE, ROGUE_SYSTEM_PRIORITY_CRITICAL, &test_system_data[0]);
    uint32_t base_id = rogue_integration_register_system(&base_desc);
    assert(base_id != 0);

    RogueSystemDescriptor dep_desc =
        create_test_system("DependentSystem", ROGUE_SYSTEM_TYPE_CONTENT,
                           ROGUE_SYSTEM_PRIORITY_IMPORTANT, &test_system_data[1]);
    dep_desc.hard_dependencies[0] = base_id;
    dep_desc.hard_dep_count = 1;
    uint32_t dep_id = rogue_integration_register_system(&dep_desc);
    assert(dep_id != 0);

    /* Test dependency validation */
    bool valid = rogue_integration_validate_dependencies();
    assert(valid == true);

    /* Test dependency graph building */
    bool graph_built = rogue_integration_build_dependency_graph();
    assert(graph_built == true);

    /* Test initialization order */
    uint32_t order[2];
    bool order_result = rogue_integration_get_initialization_order(order, 2);
    assert(order_result == true);
    assert(order[0] == base_id); /* Base system should come first */
    assert(order[1] == dep_id);  /* Dependent system should come second */

    rogue_integration_manager_shutdown();
    printf("  ✓ Dependency management passed\n");
    return true;
}

static bool test_health_monitoring(void)
{
    printf("Testing health monitoring...\n");

    rogue_integration_manager_init();

    RogueSystemDescriptor desc = create_test_system(
        "HealthTest", ROGUE_SYSTEM_TYPE_UI, ROGUE_SYSTEM_PRIORITY_OPTIONAL, &test_system_data[0]);
    uint32_t system_id = rogue_integration_register_system(&desc);
    assert(system_id != 0);

    /* Initially not healthy (not running) */
    bool healthy = rogue_integration_is_system_healthy(system_id);
    assert(healthy == false);

    /* Initialize and check health */
    rogue_integration_initialize_system(system_id);
    healthy = rogue_integration_is_system_healthy(system_id);
    assert(healthy == true);

    /* Test health report generation */
    char health_report[1024];
    rogue_integration_get_health_report(health_report, sizeof(health_report));
    assert(strlen(health_report) > 0);
    assert(strstr(health_report, "HealthTest") != NULL);

    rogue_integration_manager_shutdown();
    printf("  ✓ Health monitoring passed\n");
    return true;
}

static bool test_performance_monitoring(void)
{
    printf("Testing performance monitoring...\n");

    rogue_integration_manager_init();

    /* Reset performance counters */
    rogue_integration_reset_performance_counters();
    assert(rogue_integration_get_average_update_time_ms() == 0.0);
    assert(rogue_integration_get_max_update_time_ms() == 0.0);

    RogueSystemDescriptor desc = create_test_system(
        "PerfTest", ROGUE_SYSTEM_TYPE_CORE, ROGUE_SYSTEM_PRIORITY_CRITICAL, &test_system_data[0]);
    uint32_t system_id = rogue_integration_register_system(&desc);
    rogue_integration_initialize_system(system_id);

    /* Perform several update cycles */
    for (int i = 0; i < 10; i++)
    {
        rogue_integration_manager_update(16.67);
    }

    /* Check that performance metrics were updated */
    double avg_time = rogue_integration_get_average_update_time_ms();
    double max_time = rogue_integration_get_max_update_time_ms();
    assert(avg_time >= 0.0);
    assert(max_time >= 0.0);

    rogue_integration_manager_shutdown();
    printf("  ✓ Performance monitoring passed\n");
    return true;
}

static bool test_system_capabilities(void)
{
    printf("Testing system capability checking...\n");

    rogue_integration_manager_init();

    RogueSystemDescriptor desc =
        create_test_system("CapabilityTest", ROGUE_SYSTEM_TYPE_CONTENT,
                           ROGUE_SYSTEM_PRIORITY_IMPORTANT, &test_system_data[0]);
    desc.capabilities = ROGUE_SYSTEM_CAP_REQUIRES_UPDATE | ROGUE_SYSTEM_CAP_CONFIGURABLE |
                        ROGUE_SYSTEM_CAP_SERIALIZABLE;
    uint32_t system_id = rogue_integration_register_system(&desc);

    /* Test capability checks */
    assert(rogue_integration_has_capability(system_id, ROGUE_SYSTEM_CAP_REQUIRES_UPDATE) == true);
    assert(rogue_integration_has_capability(system_id, ROGUE_SYSTEM_CAP_CONFIGURABLE) == true);
    assert(rogue_integration_has_capability(system_id, ROGUE_SYSTEM_CAP_SERIALIZABLE) == true);
    assert(rogue_integration_has_capability(system_id, ROGUE_SYSTEM_CAP_REQUIRES_RENDERING) ==
           false);

    rogue_integration_manager_shutdown();
    printf("  ✓ System capability checking passed\n");
    return true;
}

static bool test_utility_functions(void)
{
    printf("Testing utility functions...\n");

    /* Test type name functions */
    assert(strcmp(rogue_integration_system_type_name(ROGUE_SYSTEM_TYPE_CORE), "Core") == 0);
    assert(strcmp(rogue_integration_system_type_name(ROGUE_SYSTEM_TYPE_UI), "UI") == 0);
    assert(strcmp(rogue_integration_system_type_name(ROGUE_SYSTEM_TYPE_COUNT), "Unknown") == 0);

    assert(strcmp(rogue_integration_system_priority_name(ROGUE_SYSTEM_PRIORITY_CRITICAL),
                  "Critical") == 0);
    assert(strcmp(rogue_integration_system_priority_name(ROGUE_SYSTEM_PRIORITY_OPTIONAL),
                  "Optional") == 0);

    assert(strcmp(rogue_integration_system_state_name(ROGUE_SYSTEM_STATE_RUNNING), "Running") == 0);
    assert(strcmp(rogue_integration_system_state_name(ROGUE_SYSTEM_STATE_FAILED), "Failed") == 0);

    printf("  ✓ Utility functions passed\n");
    return true;
}

int main(void)
{
    printf("Running Integration Manager Unit Tests\n");
    printf("=====================================\n");

    bool all_passed = true;

    all_passed &= test_manager_initialization();
    all_passed &= test_system_registration();
    all_passed &= test_system_lifecycle();
    all_passed &= test_failed_system_handling();
    all_passed &= test_dependency_management();
    all_passed &= test_health_monitoring();
    all_passed &= test_performance_monitoring();
    all_passed &= test_system_capabilities();
    all_passed &= test_utility_functions();

    printf("=====================================\n");
    if (all_passed)
    {
        printf("All tests PASSED! ✓\n");
        return 0;
    }
    else
    {
        printf("Some tests FAILED! ✗\n");
        return 1;
    }
}
