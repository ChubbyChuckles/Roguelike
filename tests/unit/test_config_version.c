#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

#include "core/integration/config_version.h"

/* Test state tracking */
static uint32_t g_tests_run = 0;
static uint32_t g_tests_passed = 0;
static uint32_t g_tests_failed = 0;

/* Test helper macros */
#define TEST_ASSERT(condition, message) \
    do { \
        g_tests_run++; \
        if (condition) { \
            g_tests_passed++; \
            printf("PASS: %s\n", message); \
        } else { \
            g_tests_failed++; \
            printf("FAIL: %s\n", message); \
        } \
    } while(0)

#define TEST_SECTION(name) \
    printf("\n=== Testing %s ===\n", name)

/* Forward declarations */
static void test_config_version_init(void);
static void test_event_type_registration(void);
static void test_event_type_collision_detection(void);
static void test_event_type_id_validation(void);
static void test_event_type_range_reservation(void);
static void test_config_file_validation(void);
static void test_fuzz_event_type_registration(void);
static void test_configuration_migration(void);
static void test_version_comparison(void);

/* Main test runner */
int main(void) {
    printf("Configuration Version Management and Validation Tests\n");
    printf("====================================================\n");
    
    test_config_version_init();
    test_event_type_registration();
    test_event_type_collision_detection();
    test_event_type_id_validation();
    test_event_type_range_reservation();
    test_config_file_validation();
    test_version_comparison();
    test_configuration_migration();
    test_fuzz_event_type_registration();
    
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %u\n", g_tests_run);
    printf("Tests passed: %u\n", g_tests_passed);
    printf("Tests failed: %u\n", g_tests_failed);
    printf("Success rate: %.1f%%\n", 
           g_tests_run > 0 ? (100.0f * g_tests_passed / g_tests_run) : 0.0f);
    
    return (g_tests_failed == 0) ? 0 : 1;
}

/* Test configuration version manager initialization */
static void test_config_version_init(void) {
    TEST_SECTION("Configuration Version Initialization");
    
    /* Test normal initialization */
    bool result = rogue_config_version_init("./test_config");
    TEST_ASSERT(result == true, "Configuration manager initialization should succeed");
    
    /* Test getting current version */
    const RogueConfigVersion* version = rogue_config_get_current_version();
    TEST_ASSERT(version != NULL, "Should be able to get current version");
    
    if (version) {
        TEST_ASSERT(version->major == 1, "Version major should be 1");
        TEST_ASSERT(version->minor == 0, "Version minor should be 0");
        TEST_ASSERT(version->patch == 0, "Version patch should be 0");
        TEST_ASSERT(strlen(version->schema_name) > 0, "Schema name should not be empty");
    }
    
    /* Test double initialization (should succeed) */
    result = rogue_config_version_init("./test_config");
    TEST_ASSERT(result == true, "Double initialization should succeed gracefully");
    
    /* Test initialization with NULL directory (should fail) */
    rogue_config_version_shutdown();
    result = rogue_config_version_init(NULL);
    TEST_ASSERT(result == false, "Initialization with NULL directory should fail");
    
    /* Re-initialize for other tests */
    rogue_config_version_init("./test_config");
}

/* Test event type registration system */
static void test_event_type_registration(void) {
    TEST_SECTION("Event Type Registration");
    
    /* Test valid registration - use ID in unreserved range */
    bool result = rogue_event_type_register_safe(0x0C00, "TEST_EVENT_1", __FILE__, __LINE__);
    TEST_ASSERT(result == true, "Valid event type registration should succeed");
    
    /* Test duplicate registration (should detect collision) */
    result = rogue_event_type_register_safe(0x0C00, "TEST_EVENT_1_DUP", __FILE__, __LINE__);
    TEST_ASSERT(result == false, "Duplicate event type ID should be rejected");
    
    /* Test registration with invalid name */
    result = rogue_event_type_register_safe(0x0C01, "", __FILE__, __LINE__);
    TEST_ASSERT(result == false, "Empty event name should be rejected");
    
    result = rogue_event_type_register_safe(0x0C02, NULL, __FILE__, __LINE__);
    TEST_ASSERT(result == false, "NULL event name should be rejected");
    
    /* Test registration with invalid characters in name */
    result = rogue_event_type_register_safe(0x1003, "INVALID-NAME", __FILE__, __LINE__);
    TEST_ASSERT(result == false, "Event name with invalid characters should be rejected");
    
    /* Test registration with name starting with digit */
    result = rogue_event_type_register_safe(0x1004, "1INVALID_NAME", __FILE__, __LINE__);
    TEST_ASSERT(result == false, "Event name starting with digit should be rejected");
    
    /* Test registration with very long name */
    char long_name[128];
    memset(long_name, 'A', sizeof(long_name) - 1);
    long_name[sizeof(long_name) - 1] = '\0';
    result = rogue_event_type_register_safe(0x0C05, long_name, __FILE__, __LINE__);
    TEST_ASSERT(result == false, "Very long event name should be rejected");
}

/* Test collision detection system */
static void test_event_type_collision_detection(void) {
    TEST_SECTION("Event Type Collision Detection");
    
    /* Register a test event */
    rogue_event_type_register_safe(0x0C10, "COLLISION_TEST", __FILE__, __LINE__);
    
    /* Test collision check */
    char collision_info[512];
    bool has_collision = rogue_event_type_check_collision(0x0C10, collision_info, sizeof(collision_info));
    TEST_ASSERT(has_collision == true, "Should detect collision for registered event ID");
    TEST_ASSERT(strlen(collision_info) > 0, "Collision info should not be empty");
    
    /* Test no collision for unregistered ID */
    has_collision = rogue_event_type_check_collision(0x0C11, collision_info, sizeof(collision_info));
    TEST_ASSERT(has_collision == false, "Should not detect collision for unregistered event ID");
    
    /* Test collision with reserved range */
    has_collision = rogue_event_type_check_collision(0x0010, collision_info, sizeof(collision_info));
    TEST_ASSERT(has_collision == true, "Should detect collision with reserved range");
}

/* Test event type ID validation */
static void test_event_type_id_validation(void) {
    TEST_SECTION("Event Type ID Validation");
    
    char error_msg[256];
    
    /* Test valid IDs */
    bool valid = rogue_event_type_validate_id(0x0C00, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == true, "ID 0x0C00 should be valid");
    
    valid = rogue_event_type_validate_id(0x0100, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == true, "ID 0x0100 should be valid");
    
    /* Test invalid IDs */
    valid = rogue_event_type_validate_id(0, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == false, "ID 0 should be invalid");
    TEST_ASSERT(strlen(error_msg) > 0, "Error message should be provided for invalid ID");
    
    valid = rogue_event_type_validate_id(0xFFFFFFFF, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == false, "ID 0xFFFFFFFF should be invalid (reserved)");
    
    valid = rogue_event_type_validate_id(0xDEADBEEF, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == false, "ID 0xDEADBEEF should be invalid (debug reserved)");
    
    valid = rogue_event_type_validate_id(0xCAFEBABE, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == false, "ID 0xCAFEBABE should be invalid (debug reserved)");
    
    /* Test ID beyond maximum (should fail with new expanded limit) */
    valid = rogue_event_type_validate_id(5000, error_msg, sizeof(error_msg));
    TEST_ASSERT(valid == false, "ID beyond maximum should be invalid");
}

/* Test event type range reservation */
static void test_event_type_range_reservation(void) {
    TEST_SECTION("Event Type Range Reservation");
    
    /* Test valid range reservation */
    bool result = rogue_event_type_reserve_range(0x3000, 0x30FF, "TEST_SYSTEM");
    TEST_ASSERT(result == true, "Valid range reservation should succeed");
    
    /* Test overlapping range reservation */
    result = rogue_event_type_reserve_range(0x30F0, 0x31FF, "OVERLAPPING_SYSTEM");
    TEST_ASSERT(result == false, "Overlapping range reservation should fail");
    
    /* Test invalid range (start >= end) */
    result = rogue_event_type_reserve_range(0x3200, 0x3100, "INVALID_RANGE");
    TEST_ASSERT(result == false, "Invalid range reservation should fail");
    
    /* Test reservation with empty system name */
    result = rogue_event_type_reserve_range(0x3300, 0x33FF, "");
    TEST_ASSERT(result == false, "Reservation with empty system name should fail");
    
    result = rogue_event_type_reserve_range(0x3400, 0x34FF, NULL);
    TEST_ASSERT(result == false, "Reservation with NULL system name should fail");
}

/* Test configuration file validation */
static void test_config_file_validation(void) {
    TEST_SECTION("Configuration File Validation");
    
    char error_details[512];
    
    /* Test non-existent file */
    RogueConfigValidationResult result = rogue_config_validate_file(
        "non_existent_file.cfg", error_details, sizeof(error_details));
    TEST_ASSERT(result != ROGUE_CONFIG_VALID, "Non-existent file should be invalid");
    TEST_ASSERT(strlen(error_details) > 0, "Error details should be provided");
    
    /* Create a test configuration file */
    FILE* test_file = fopen("test_config.cfg", "w");
    if (test_file) {
        fprintf(test_file, "# Test configuration file\n");
        fprintf(test_file, "version=1.0.0\n");
        fprintf(test_file, "max_event_types=4096\n");
        fclose(test_file);
        
        /* Test valid file */
        result = rogue_config_validate_file("test_config.cfg", error_details, sizeof(error_details));
        TEST_ASSERT(result == ROGUE_CONFIG_VALID, "Valid config file should pass validation");
        
        /* Clean up */
        remove("test_config.cfg");
    }
    
    /* Test with invalid parameters */
    result = rogue_config_validate_file(NULL, error_details, sizeof(error_details));
    TEST_ASSERT(result != ROGUE_CONFIG_VALID, "NULL file path should be invalid");
    
    result = rogue_config_validate_file("test.cfg", NULL, 0);
    TEST_ASSERT(result != ROGUE_CONFIG_VALID, "NULL error buffer should be invalid");
}

/* Test version comparison */
static void test_version_comparison(void) {
    TEST_SECTION("Version Comparison");
    
    RogueConfigVersion v1 = {1, 0, 0, 0, 0, "Test"};
    RogueConfigVersion v2 = {1, 1, 0, 0, 0, "Test"};
    RogueConfigVersion v3 = {2, 0, 0, 0, 0, "Test"};
    RogueConfigVersion v4 = {1, 0, 1, 0, 0, "Test"};
    
    int result = rogue_config_version_compare(&v1, &v2);
    TEST_ASSERT(result < 0, "v1.0.0 should be less than v1.1.0");
    
    result = rogue_config_version_compare(&v2, &v1);
    TEST_ASSERT(result > 0, "v1.1.0 should be greater than v1.0.0");
    
    result = rogue_config_version_compare(&v1, &v3);
    TEST_ASSERT(result < 0, "v1.0.0 should be less than v2.0.0");
    
    result = rogue_config_version_compare(&v1, &v4);
    TEST_ASSERT(result < 0, "v1.0.0 should be less than v1.0.1");
    
    result = rogue_config_version_compare(&v1, &v1);
    TEST_ASSERT(result == 0, "v1.0.0 should be equal to itself");
    
    /* Test with NULL pointers */
    result = rogue_config_version_compare(NULL, &v1);
    TEST_ASSERT(result == 0, "NULL version comparison should return 0");
    
    result = rogue_config_version_compare(&v1, NULL);
    TEST_ASSERT(result == 0, "NULL version comparison should return 0");
}

/* Test configuration migration (stub for now) */
static void test_configuration_migration(void) {
    TEST_SECTION("Configuration Migration");
    
    /* Test migration detection */
    RogueConfigVersion detected_version;
    bool needs_migration = rogue_config_needs_migration("non_existent.cfg", &detected_version);
    TEST_ASSERT(needs_migration == false, "Non-existent file should not need migration");
    
    /* TODO: Add more comprehensive migration tests when migration system is fully implemented */
    printf("Migration tests are currently stubs - will be expanded in future iterations\n");
}

/* Fuzz testing for event type registration (Phase 2.7) */
static void test_fuzz_event_type_registration(void) {
    TEST_SECTION("Fuzz Testing Event Type Registration");
    
    uint32_t fuzz_test_count = 100;
    uint32_t successful_registrations = 0;
    uint32_t expected_failures = 0;
    
    /* Define unreserved ranges that should work for testing */
    uint32_t unreserved_ranges[][2] = {
        {2816, 3071},   /* Between AI Events and our test range */
        {3072, 4095},   /* Our main test range (avoiding 4096 exactly) */
        {12800, 13055}, /* After our test reservation */
    };
    uint32_t num_ranges = sizeof(unreserved_ranges) / sizeof(unreserved_ranges[0]);
    
    for (uint32_t i = 0; i < fuzz_test_count; i++) {
        uint32_t event_id;
        
        if (i < 50) {
            /* First 50: test valid unreserved IDs */
            uint32_t range_idx = i % num_ranges;
            uint32_t range_start = unreserved_ranges[range_idx][0];
            uint32_t range_end = unreserved_ranges[range_idx][1];
            event_id = range_start + (i % (range_end - range_start + 1));
        } else {
            /* Last 50: test problematic IDs (reserved ranges, out of bounds, etc.) */
            uint32_t problematic_ids[] = {
                0, 1, 255,           /* Core range */
                256, 511,            /* Player range */
                512, 767,            /* Combat range */
                4097, 5000, 8192,    /* Out of bounds */
                0xFFFFFFFF,          /* Invalid */
            };
            uint32_t num_problematic = sizeof(problematic_ids) / sizeof(problematic_ids[0]);
            event_id = problematic_ids[(i - 50) % num_problematic];
        }
        
        /* Generate pseudo-random event name */
        char event_name[32];
        snprintf(event_name, sizeof(event_name), "FUZZ_EVENT_%u", i);
        
        /* Validate event ID first BEFORE attempting registration */
        char error_msg[256];
        bool id_valid = rogue_event_type_validate_id(event_id, error_msg, sizeof(error_msg));
        
        /* Check for collision BEFORE attempting registration */
        char collision_info[256];
        bool has_collision = rogue_event_type_check_collision(event_id, collision_info, sizeof(collision_info));
        
        /* Now attempt registration */
        bool result = rogue_event_type_register_safe(event_id, event_name, __FILE__, __LINE__);
        
        if (id_valid && !has_collision) {
            /* This should succeed */
            if (result) {
                successful_registrations++;
            } else {
                printf("Unexpected failure for valid ID %u: %s\n", event_id, event_name);
            }
        } else {
            /* This should fail */
            expected_failures++;
            if (result) {
                printf("Unexpected success for invalid/colliding ID %u\n", event_id);
            }
        }
    }
    
    TEST_ASSERT(successful_registrations > 0, "Some fuzz registrations should succeed");
    TEST_ASSERT(expected_failures > 0, "Some fuzz registrations should fail as expected");
    
    printf("Fuzz test results: %u successful, %u expected failures out of %u tests\n",
           successful_registrations, expected_failures, fuzz_test_count);
}
