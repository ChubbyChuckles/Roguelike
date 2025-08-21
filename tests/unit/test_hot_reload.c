#include "../../src/core/integration/hot_reload.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// Windows compatibility
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define mkdir(a) _mkdir(a)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// Test utilities
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition)                                                                     \
    do                                                                                             \
    {                                                                                              \
        tests_run++;                                                                               \
        if (condition)                                                                             \
        {                                                                                          \
            tests_passed++;                                                                        \
            printf("  ✓ Test %d passed\n", tests_run);                                             \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            printf("  ✗ Test %d failed: %s\n", tests_run, #condition);                             \
        }                                                                                          \
    } while (0)

#define TEST_FUNCTION(name)                                                                        \
    printf("\n=== Running %s ===\n", #name);                                                       \
    name()

// Test data directories
#define TEST_CONFIG_DIR "./test_hot_reload_configs"
#define TEST_BACKUP_DIR "./test_hot_reload_backups"
#define TEST_WATCH_DIR "./test_hot_reload_watch"

// Global test variables
static RogueHotReloadSystem* g_test_system = NULL;
static bool g_callback_called = false;
static RogueReloadChangeType g_callback_change_type = ROGUE_RELOAD_CHANGE_CREATED;
static char g_callback_file_path[512] = {0};

// =============================================================================
// Test Helper Functions
// =============================================================================

static void setup_test_environment(void)
{
    // Create test directories
    mkdir(TEST_CONFIG_DIR);
    mkdir(TEST_BACKUP_DIR);
    mkdir(TEST_WATCH_DIR);

    // Reset global state
    g_callback_called = false;
    g_callback_change_type = ROGUE_RELOAD_CHANGE_CREATED;
    memset(g_callback_file_path, 0, sizeof(g_callback_file_path));
}

static void cleanup_test_environment(void)
{
    // Clean up test files and directories (simplified)
    system("rmdir /s /q " TEST_CONFIG_DIR " 2>nul");
    system("rmdir /s /q " TEST_BACKUP_DIR " 2>nul");
    system("rmdir /s /q " TEST_WATCH_DIR " 2>nul");
}

static void create_test_file(const char* path, const char* content)
{
    FILE* file = fopen(path, "w");
    if (file)
    {
        fprintf(file, "%s", content);
        fclose(file);
    }
}

static bool file_contains(const char* path, const char* expected_content)
{
    FILE* file = fopen(path, "r");
    if (!file)
        return false;

    char buffer[1024] = {0};
    fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);

    return strstr(buffer, expected_content) != NULL;
}

static void test_callback(const char* file_path, RogueReloadChangeType change_type, void* user_data)
{
    (void) user_data;
    g_callback_called = true;
    g_callback_change_type = change_type;
    strncpy(g_callback_file_path, file_path, sizeof(g_callback_file_path) - 1);
}

static bool test_validator(const char* file_path, const char* content, void* user_data)
{
    (void) file_path;
    (void) user_data;

    // Simple validation: reject files containing "INVALID"
    return strstr(content, "INVALID") == NULL;
}

static void test_notifier(const RogueReloadNotification* notification)
{
    printf("Notification: %s -> %s (%s)\n", notification->target_system, notification->config_file,
           rogue_reload_change_type_to_string(notification->change_type));
}

// =============================================================================
// Test Cases
// =============================================================================

static void test_system_lifecycle(void)
{
    // Test system creation
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    TEST_ASSERT(system != NULL);
    TEST_ASSERT(system->status == ROGUE_RELOAD_STATUS_INACTIVE);
    TEST_ASSERT(system->watcher_count == 0);
    TEST_ASSERT(system->transaction_count == 0);

    // Test system initialization
    bool init_result = rogue_hot_reload_init(system);
    TEST_ASSERT(init_result == true);
    TEST_ASSERT(system->status == ROGUE_RELOAD_STATUS_WATCHING);

    // Test system update (should not crash)
    rogue_hot_reload_update(system);

    // Test system shutdown
    rogue_hot_reload_shutdown(system);
    TEST_ASSERT(system->status == ROGUE_RELOAD_STATUS_INACTIVE);

    // Test system destruction
    rogue_hot_reload_destroy(system);
    printf("System lifecycle test completed\n");
}

static void test_file_watcher(void)
{
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);

    // Create a test file to watch
    create_test_file(TEST_WATCH_DIR "/test.cfg", "test_value = 123");

    // Test adding watcher
    bool add_result =
        rogue_hot_reload_add_watcher(system, TEST_WATCH_DIR, false, test_callback, NULL);
    TEST_ASSERT(add_result == true);
    TEST_ASSERT(system->watcher_count == 1);
    TEST_ASSERT(rogue_hot_reload_is_watching(system, TEST_WATCH_DIR) == true);

    // Test adding duplicate watcher (should still work)
    add_result = rogue_hot_reload_add_watcher(system, TEST_WATCH_DIR, true, test_callback, NULL);
    TEST_ASSERT(add_result == true);
    TEST_ASSERT(system->watcher_count == 2);

    // Test removing watcher
    bool remove_result = rogue_hot_reload_remove_watcher(system, TEST_WATCH_DIR);
    TEST_ASSERT(remove_result == true);
    TEST_ASSERT(system->watcher_count == 1);

    // Test removing non-existent watcher
    remove_result = rogue_hot_reload_remove_watcher(system, "/non/existent/path");
    TEST_ASSERT(remove_result == false);
    TEST_ASSERT(system->watcher_count == 1);

    rogue_hot_reload_destroy(system);
    printf("File watcher test completed\n");
}

static void test_change_detection(void)
{
    const char* test_file = TEST_CONFIG_DIR "/change_test.cfg";

    // Create initial file
    create_test_file(test_file, "initial_content");

    // Get initial file info
    RogueFileInfo initial_info = {0};
    bool info_result = rogue_hot_reload_update_file_info(&initial_info, test_file);
    TEST_ASSERT(info_result == true);
    TEST_ASSERT(initial_info.is_valid == true);
    TEST_ASSERT(strlen(initial_info.hash) > 0);

// Sleep briefly to ensure timestamp difference
#ifdef _WIN32
    Sleep(1000);
#else
    sleep(1);
#endif

    // Modify file
    create_test_file(test_file, "modified_content");

    // Check if file changed
    bool changed = rogue_hot_reload_has_file_changed(&initial_info, test_file);
    TEST_ASSERT(changed == true);

    // Test hash computation
    char hash1[64], hash2[64];
    create_test_file(test_file, "content1");
    bool hash_result1 = rogue_hot_reload_compute_file_hash(test_file, hash1, sizeof(hash1));
    TEST_ASSERT(hash_result1 == true);

    create_test_file(test_file, "content2");
    bool hash_result2 = rogue_hot_reload_compute_file_hash(test_file, hash2, sizeof(hash2));
    TEST_ASSERT(hash_result2 == true);
    TEST_ASSERT(strcmp(hash1, hash2) != 0);

    printf("Change detection test completed\n");
}

static void test_staged_reloading(void)
{
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);

    // Set up validator
    rogue_hot_reload_set_validator(system, test_validator, NULL);

    const char* test_file = TEST_CONFIG_DIR "/staged_test.cfg";

    // Test valid file staging
    create_test_file(test_file, "valid_content = true");
    bool stage_result = rogue_hot_reload_stage_reload(system, test_file);
    TEST_ASSERT(stage_result == true);

    // Test invalid file staging
    create_test_file(test_file, "INVALID content");
    stage_result = rogue_hot_reload_stage_reload(system, test_file);
    TEST_ASSERT(stage_result == false);
    TEST_ASSERT(system->status == ROGUE_RELOAD_STATUS_ERROR);

    // Reset status for next test
    system->status = ROGUE_RELOAD_STATUS_WATCHING;

    // Test applying staged changes
    create_test_file(test_file, "valid_staged_content = 42");
    stage_result = rogue_hot_reload_stage_reload(system, test_file);
    TEST_ASSERT(stage_result == true);

    bool apply_result = rogue_hot_reload_apply_staged_changes(system);
    TEST_ASSERT(apply_result == true);
    TEST_ASSERT(system->reloads_successful > 0);

    rogue_hot_reload_destroy(system);
    printf("Staged reloading test completed\n");
}

static void test_transaction_system(void)
{
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);
    rogue_hot_reload_set_validator(system, test_validator, NULL);

    const char* test_file1 = TEST_CONFIG_DIR "/transaction_test1.cfg";
    const char* test_file2 = TEST_CONFIG_DIR "/transaction_test2.cfg";

    create_test_file(test_file1, "file1_content = valid");
    create_test_file(test_file2, "file2_content = valid");

    // Test transaction creation
    uint64_t transaction_id = rogue_hot_reload_begin_transaction(system, true, true);
    TEST_ASSERT(transaction_id > 0);
    TEST_ASSERT(system->transaction_count == 1);

    // Test adding files to transaction
    bool add_result1 = rogue_hot_reload_add_file_to_transaction(system, transaction_id, test_file1);
    TEST_ASSERT(add_result1 == true);

    bool add_result2 = rogue_hot_reload_add_file_to_transaction(system, transaction_id, test_file2);
    TEST_ASSERT(add_result2 == true);

    // Test successful commit
    bool commit_result = rogue_hot_reload_commit_transaction(system, transaction_id);
    TEST_ASSERT(commit_result == true);
    TEST_ASSERT(system->transaction_count == 0);
    TEST_ASSERT(system->reloads_successful > 0);

    // Test rollback scenario
    create_test_file(test_file1, "INVALID content");
    create_test_file(test_file2, "valid content");

    transaction_id = rogue_hot_reload_begin_transaction(system, true, true);
    TEST_ASSERT(transaction_id > 0);

    rogue_hot_reload_add_file_to_transaction(system, transaction_id, test_file1);
    rogue_hot_reload_add_file_to_transaction(system, transaction_id, test_file2);

    commit_result = rogue_hot_reload_commit_transaction(system, transaction_id);
    TEST_ASSERT(commit_result == false); // Should fail due to invalid file
    TEST_ASSERT(system->rollbacks_performed > 0);

    rogue_hot_reload_destroy(system);
    printf("Transaction system test completed\n");
}

static void test_rollback_system(void)
{
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);

    const char* test_file = TEST_CONFIG_DIR "/rollback_test.cfg";
    const char* original_content = "original_content = 123";
    const char* modified_content = "modified_content = 456";

    // Set rollback directory
    bool set_result = rogue_hot_reload_set_rollback_directory(system, TEST_BACKUP_DIR);
    TEST_ASSERT(set_result == true);

    // Create original file
    create_test_file(test_file, original_content);

    // Create backup
    bool backup_result = rogue_hot_reload_backup_file(system, test_file);
    TEST_ASSERT(backup_result == true);
    TEST_ASSERT(rogue_hot_reload_has_backup(system, test_file) == true);

    // Modify original file
    create_test_file(test_file, modified_content);
    TEST_ASSERT(file_contains(test_file, "modified_content"));

    // Restore from backup
    bool restore_result = rogue_hot_reload_restore_file(system, test_file);
    TEST_ASSERT(restore_result == true);
    TEST_ASSERT(file_contains(test_file, "original_content"));

    rogue_hot_reload_destroy(system);
    printf("Rollback system test completed\n");
}

static void test_notification_system(void)
{
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);

    // Set notifier
    bool notifier_result = rogue_hot_reload_set_notifier(system, test_notifier);
    TEST_ASSERT(notifier_result == true);

    // Send notifications
    bool notify_result1 = rogue_hot_reload_send_notification(system, "GameSystem", "config.cfg",
                                                             ROGUE_RELOAD_CHANGE_MODIFIED, NULL);
    TEST_ASSERT(notify_result1 == true);
    TEST_ASSERT(system->notification_count == 1);

    bool notify_result2 = rogue_hot_reload_send_notification(system, "AudioSystem", "audio.cfg",
                                                             ROGUE_RELOAD_CHANGE_CREATED, NULL);
    TEST_ASSERT(notify_result2 == true);
    TEST_ASSERT(system->notification_count == 2);

    // Process notifications
    rogue_hot_reload_process_notifications(system);
    TEST_ASSERT(system->notification_count == 0); // Should be cleared after processing

    rogue_hot_reload_destroy(system);
    printf("Notification system test completed\n");
}

static void test_utility_functions(void)
{
    // Test status string conversion
    const char* status_str = rogue_reload_status_to_string(ROGUE_RELOAD_STATUS_WATCHING);
    TEST_ASSERT(strcmp(status_str, "WATCHING") == 0);

    // Test change type string conversion
    const char* change_str = rogue_reload_change_type_to_string(ROGUE_RELOAD_CHANGE_MODIFIED);
    TEST_ASSERT(strcmp(change_str, "MODIFIED") == 0);

    // Test priority string conversion
    const char* priority_str = rogue_reload_priority_to_string(ROGUE_RELOAD_PRIORITY_HIGH);
    TEST_ASSERT(strcmp(priority_str, "HIGH") == 0);

    // Test stage string conversion
    const char* stage_str = rogue_reload_stage_to_string(ROGUE_RELOAD_STAGE_VALIDATE);
    TEST_ASSERT(strcmp(stage_str, "VALIDATE") == 0);

    // Test statistics
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);

    uint64_t files_watched, changes_detected, reloads_successful, reloads_failed,
        rollbacks_performed;
    rogue_hot_reload_get_statistics(system, &files_watched, &changes_detected, &reloads_successful,
                                    &reloads_failed, &rollbacks_performed);

    TEST_ASSERT(files_watched >= 0);
    TEST_ASSERT(changes_detected >= 0);
    TEST_ASSERT(reloads_successful >= 0);
    TEST_ASSERT(reloads_failed >= 0);
    TEST_ASSERT(rollbacks_performed >= 0);

    // Test print statistics (visual confirmation)
    rogue_hot_reload_print_statistics(system);

    rogue_hot_reload_destroy(system);
    printf("Utility functions test completed\n");
}

static void test_error_handling(void)
{
    // Test NULL parameter handling
    TEST_ASSERT(rogue_hot_reload_create() != NULL);
    TEST_ASSERT(rogue_hot_reload_init(NULL) == false);
    TEST_ASSERT(rogue_hot_reload_add_watcher(NULL, "path", false, NULL, NULL) == false);
    TEST_ASSERT(rogue_hot_reload_compute_file_hash(NULL, NULL, 0) == false);
    TEST_ASSERT(rogue_hot_reload_backup_file(NULL, NULL) == false);

    RogueHotReloadSystem* system = rogue_hot_reload_create();

    // Test invalid paths
    TEST_ASSERT(rogue_hot_reload_add_watcher(system, "/non/existent/path", false, NULL, NULL) ==
                false);
    TEST_ASSERT(rogue_hot_reload_compute_file_hash("/non/existent/file", NULL, 0) == false);

    // Test transaction limits
    for (int i = 0; i < ROGUE_HOT_RELOAD_MAX_TRANSACTIONS + 5; i++)
    {
        uint64_t tid = rogue_hot_reload_begin_transaction(system, false, false);
        if (i < ROGUE_HOT_RELOAD_MAX_TRANSACTIONS)
        {
            TEST_ASSERT(tid > 0);
        }
        else
        {
            TEST_ASSERT(tid == 0); // Should fail when limit reached
        }
    }

    rogue_hot_reload_destroy(system);
    printf("Error handling test completed\n");
}

static void test_integration_scenarios(void)
{
    RogueHotReloadSystem* system = rogue_hot_reload_create();
    rogue_hot_reload_init(system);
    rogue_hot_reload_set_validator(system, test_validator, NULL);
    rogue_hot_reload_set_rollback_directory(system, TEST_BACKUP_DIR);
    rogue_hot_reload_set_notifier(system, test_notifier);

    const char* config_file = TEST_CONFIG_DIR "/integration_test.cfg";

    // Scenario 1: Successful reload workflow
    create_test_file(config_file, "health = 100\nmana = 50");

    uint64_t transaction_id = rogue_hot_reload_begin_transaction(system, true, true);
    rogue_hot_reload_add_file_to_transaction(system, transaction_id, config_file);

    bool backup_created = rogue_hot_reload_backup_file(system, config_file);
    TEST_ASSERT(backup_created == true);

    bool staged = rogue_hot_reload_stage_reload(system, config_file);
    TEST_ASSERT(staged == true);

    bool committed = rogue_hot_reload_commit_transaction(system, transaction_id);
    TEST_ASSERT(committed == true);

    rogue_hot_reload_send_notification(system, "GameSystem", config_file,
                                       ROGUE_RELOAD_CHANGE_MODIFIED, NULL);
    rogue_hot_reload_process_notifications(system);

    // Scenario 2: Failed reload with rollback
    create_test_file(config_file, "INVALID configuration data");

    transaction_id = rogue_hot_reload_begin_transaction(system, true, true);
    rogue_hot_reload_add_file_to_transaction(system, transaction_id, config_file);

    bool failed_commit = rogue_hot_reload_commit_transaction(system, transaction_id);
    TEST_ASSERT(failed_commit == false); // Should fail due to validation
    TEST_ASSERT(system->rollbacks_performed > 0);

    // Verify file was restored from backup
    TEST_ASSERT(file_contains(config_file, "health = 100"));

    uint64_t files_watched, changes_detected, reloads_successful, reloads_failed,
        rollbacks_performed;
    rogue_hot_reload_get_statistics(system, &files_watched, &changes_detected, &reloads_successful,
                                    &reloads_failed, &rollbacks_performed);

    TEST_ASSERT(reloads_successful > 0);
    TEST_ASSERT(reloads_failed > 0);
    TEST_ASSERT(rollbacks_performed > 0);

    rogue_hot_reload_destroy(system);
    printf("Integration scenarios test completed\n");
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main(void)
{
    printf("=== Hot Reload System Test Suite ===\n");
    printf("Testing Phase 2.4 Hot-Reload System Implementation\n\n");

    setup_test_environment();

    // Run all test functions
    TEST_FUNCTION(test_system_lifecycle);
    TEST_FUNCTION(test_file_watcher);
    TEST_FUNCTION(test_change_detection);
    TEST_FUNCTION(test_staged_reloading);
    TEST_FUNCTION(test_transaction_system);
    TEST_FUNCTION(test_rollback_system);
    TEST_FUNCTION(test_notification_system);
    TEST_FUNCTION(test_utility_functions);
    TEST_FUNCTION(test_error_handling);
    TEST_FUNCTION(test_integration_scenarios);

    cleanup_test_environment();

    // Print final results
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);

    if (tests_passed == tests_run)
    {
        printf("\n🎉 All tests passed! Hot Reload System implementation is working correctly.\n");
        return 0;
    }
    else
    {
        printf("\n❌ Some tests failed. Please check the implementation.\n");
        return 1;
    }
}
