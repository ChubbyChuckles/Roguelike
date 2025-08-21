#ifndef ROGUE_HOT_RELOAD_H
#define ROGUE_HOT_RELOAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Phase 2.4: Hot-Reload System Implementation
 * 
 * Complete hot-reload system for JSON configuration files with file system
 * watching, change detection, staged validation, and atomic updates.
 * =============================================================================*/

// =============================================================================
// Constants & Limits
// =============================================================================

#define ROGUE_HOT_RELOAD_MAX_WATCHERS 256
#define ROGUE_HOT_RELOAD_MAX_FILES 1024
#define ROGUE_HOT_RELOAD_MAX_PATH 512
#define ROGUE_HOT_RELOAD_MAX_HASH 64
#define ROGUE_HOT_RELOAD_MAX_DEPENDENCIES 64
#define ROGUE_HOT_RELOAD_MAX_TRANSACTIONS 32
#define ROGUE_HOT_RELOAD_MAX_NOTIFICATIONS 128

// =============================================================================
// Type Definitions
// =============================================================================

typedef enum {
    ROGUE_RELOAD_STATUS_INACTIVE = 0,
    ROGUE_RELOAD_STATUS_WATCHING,
    ROGUE_RELOAD_STATUS_VALIDATING,
    ROGUE_RELOAD_STATUS_APPLYING,
    ROGUE_RELOAD_STATUS_ROLLING_BACK,
    ROGUE_RELOAD_STATUS_ERROR,
    ROGUE_RELOAD_STATUS_COUNT
} RogueReloadStatus;

typedef enum {
    ROGUE_RELOAD_CHANGE_CREATED = 0,
    ROGUE_RELOAD_CHANGE_MODIFIED,
    ROGUE_RELOAD_CHANGE_DELETED,
    ROGUE_RELOAD_CHANGE_RENAMED,
    ROGUE_RELOAD_CHANGE_COUNT
} RogueReloadChangeType;

typedef enum {
    ROGUE_RELOAD_PRIORITY_LOW = 0,
    ROGUE_RELOAD_PRIORITY_NORMAL,
    ROGUE_RELOAD_PRIORITY_HIGH,
    ROGUE_RELOAD_PRIORITY_CRITICAL,
    ROGUE_RELOAD_PRIORITY_COUNT
} RogueReloadPriority;

typedef enum {
    ROGUE_RELOAD_STAGE_DETECT = 0,
    ROGUE_RELOAD_STAGE_VALIDATE,
    ROGUE_RELOAD_STAGE_STAGE,
    ROGUE_RELOAD_STAGE_APPLY,
    ROGUE_RELOAD_STAGE_NOTIFY,
    ROGUE_RELOAD_STAGE_COUNT
} RogueReloadStage;

// =============================================================================
// Forward Declarations  
// =============================================================================

typedef struct RogueHotReloadSystem RogueHotReloadSystem;
typedef struct RogueFileWatcher RogueFileWatcher;

// =============================================================================
// Core Structures
// =============================================================================

typedef struct {
    char file_path[ROGUE_HOT_RELOAD_MAX_PATH];
    char hash[ROGUE_HOT_RELOAD_MAX_HASH];
    time_t last_modified;
    size_t file_size;
    RogueReloadChangeType change_type;
    bool is_valid;
} RogueFileInfo;

typedef struct {
    char file_path[ROGUE_HOT_RELOAD_MAX_PATH];
    char old_path[ROGUE_HOT_RELOAD_MAX_PATH];  // For renames
    RogueReloadChangeType change_type;
    time_t timestamp;
    uint64_t event_id;
    bool processed;
} RogueReloadEvent;

typedef struct {
    char config_file[ROGUE_HOT_RELOAD_MAX_PATH];
    char dependencies[ROGUE_HOT_RELOAD_MAX_DEPENDENCIES][ROGUE_HOT_RELOAD_MAX_PATH];
    int dependency_count;
    RogueReloadPriority priority;
    bool is_weak_dependency;
} RogueConfigDependency;

typedef struct {
    uint64_t transaction_id;
    char files[ROGUE_HOT_RELOAD_MAX_FILES][ROGUE_HOT_RELOAD_MAX_PATH];
    int file_count;
    RogueReloadStage current_stage;
    bool is_atomic;
    bool rollback_on_failure;
    time_t start_time;
    char error_message[256];
} RogueReloadTransaction;

typedef struct {
    char target_system[64];
    char config_file[ROGUE_HOT_RELOAD_MAX_PATH];
    RogueReloadChangeType change_type;
    void* user_data;
    bool acknowledged;
} RogueReloadNotification;

// =============================================================================
// Callback Function Types
// =============================================================================

typedef void (*RogueReloadCallback)(const char* file_path, RogueReloadChangeType change_type, void* user_data);
typedef bool (*RogueReloadValidator)(const char* file_path, const char* content, void* user_data);
typedef void (*RogueReloadNotifier)(const RogueReloadNotification* notification);

// =============================================================================
// File Watcher (2.4.1)
// =============================================================================

struct RogueFileWatcher {
    char watch_path[ROGUE_HOT_RELOAD_MAX_PATH];
    bool recursive;
    bool is_active;
    RogueFileInfo files[ROGUE_HOT_RELOAD_MAX_FILES];
    int file_count;
    RogueReloadEvent events[ROGUE_HOT_RELOAD_MAX_FILES];
    int event_count;
    RogueReloadCallback callback;
    void* callback_data;
    uint64_t next_event_id;
    
    // Platform-specific data
#ifdef _WIN32
    void* directory_handle;
    void* completion_port;
    char* buffer;
    size_t buffer_size;
#else
    int inotify_fd;
    int watch_descriptor;
#endif
};

// =============================================================================
// Hot Reload System
// =============================================================================

struct RogueHotReloadSystem {
    RogueReloadStatus status;
    RogueFileWatcher watchers[ROGUE_HOT_RELOAD_MAX_WATCHERS];
    int watcher_count;
    
    // Change Detection (2.4.2)
    bool enable_hash_comparison;
    bool enable_timestamp_check;
    bool enable_size_check;
    
    // Staged Reloading (2.4.3)
    RogueReloadValidator validator;
    void* validator_data;
    bool enable_staged_reload;
    
    // Transaction System (2.4.4)
    RogueReloadTransaction transactions[ROGUE_HOT_RELOAD_MAX_TRANSACTIONS];
    int transaction_count;
    uint64_t next_transaction_id;
    
    // Dependency Management (2.4.5)
    RogueConfigDependency dependencies[ROGUE_HOT_RELOAD_MAX_FILES];
    int dependency_count;
    
    // Error Handling & Rollback (2.4.6)
    bool enable_rollback;
    char rollback_directory[ROGUE_HOT_RELOAD_MAX_PATH];
    
    // Notification System (2.4.7)
    RogueReloadNotification notifications[ROGUE_HOT_RELOAD_MAX_NOTIFICATIONS];
    int notification_count;
    RogueReloadNotifier notifier;
    
    // Statistics
    uint64_t files_watched;
    uint64_t changes_detected;
    uint64_t reloads_successful;
    uint64_t reloads_failed;
    uint64_t rollbacks_performed;
};

// =============================================================================
// Core Functions
// =============================================================================

/* System Management */
RogueHotReloadSystem* rogue_hot_reload_create(void);
bool rogue_hot_reload_init(RogueHotReloadSystem* system);
void rogue_hot_reload_update(RogueHotReloadSystem* system);
void rogue_hot_reload_shutdown(RogueHotReloadSystem* system);
void rogue_hot_reload_destroy(RogueHotReloadSystem* system);

/* File System Watcher (2.4.1) */
bool rogue_hot_reload_add_watcher(RogueHotReloadSystem* system, const char* path, bool recursive, RogueReloadCallback callback, void* user_data);
bool rogue_hot_reload_remove_watcher(RogueHotReloadSystem* system, const char* path);
void rogue_hot_reload_process_events(RogueHotReloadSystem* system);
bool rogue_hot_reload_is_watching(const RogueHotReloadSystem* system, const char* path);

/* Change Detection (2.4.2) */
bool rogue_hot_reload_compute_file_hash(const char* file_path, char* hash_out, size_t hash_size);
bool rogue_hot_reload_has_file_changed(const RogueFileInfo* old_info, const char* file_path);
bool rogue_hot_reload_update_file_info(RogueFileInfo* info, const char* file_path);
void rogue_hot_reload_set_change_detection_mode(RogueHotReloadSystem* system, bool use_hash, bool use_timestamp, bool use_size);

/* Staged Reloading (2.4.3) */
bool rogue_hot_reload_set_validator(RogueHotReloadSystem* system, RogueReloadValidator validator, void* user_data);
bool rogue_hot_reload_validate_file(RogueHotReloadSystem* system, const char* file_path);
bool rogue_hot_reload_stage_reload(RogueHotReloadSystem* system, const char* file_path);
bool rogue_hot_reload_apply_staged_changes(RogueHotReloadSystem* system);

/* Transaction System (2.4.4) */
uint64_t rogue_hot_reload_begin_transaction(RogueHotReloadSystem* system, bool atomic, bool rollback_on_failure);
bool rogue_hot_reload_add_file_to_transaction(RogueHotReloadSystem* system, uint64_t transaction_id, const char* file_path);
bool rogue_hot_reload_commit_transaction(RogueHotReloadSystem* system, uint64_t transaction_id);
bool rogue_hot_reload_rollback_transaction(RogueHotReloadSystem* system, uint64_t transaction_id);
void rogue_hot_reload_abort_all_transactions(RogueHotReloadSystem* system);

/* Selective Reloading (2.4.5) */
bool rogue_hot_reload_add_dependency(RogueHotReloadSystem* system, const char* config_file, const char* dependency, RogueReloadPriority priority, bool is_weak);
bool rogue_hot_reload_remove_dependency(RogueHotReloadSystem* system, const char* config_file, const char* dependency);
bool rogue_hot_reload_get_affected_files(const RogueHotReloadSystem* system, const char* changed_file, char affected_files[][ROGUE_HOT_RELOAD_MAX_PATH], int* count, int max_files);
bool rogue_hot_reload_reload_selective(RogueHotReloadSystem* system, const char* file_path);

/* Error Handling & Rollback (2.4.6) */
bool rogue_hot_reload_set_rollback_directory(RogueHotReloadSystem* system, const char* directory);
bool rogue_hot_reload_backup_file(RogueHotReloadSystem* system, const char* file_path);
bool rogue_hot_reload_restore_file(RogueHotReloadSystem* system, const char* file_path);
bool rogue_hot_reload_has_backup(const RogueHotReloadSystem* system, const char* file_path);

/* Notification System (2.4.7) */
bool rogue_hot_reload_set_notifier(RogueHotReloadSystem* system, RogueReloadNotifier notifier);
bool rogue_hot_reload_send_notification(RogueHotReloadSystem* system, const char* target_system, const char* config_file, RogueReloadChangeType change_type, void* user_data);
void rogue_hot_reload_process_notifications(RogueHotReloadSystem* system);
void rogue_hot_reload_acknowledge_notification(RogueHotReloadSystem* system, int notification_id);

// =============================================================================
// Utility Functions
// =============================================================================

/* Status & Diagnostics */
const char* rogue_reload_status_to_string(RogueReloadStatus status);
const char* rogue_reload_change_type_to_string(RogueReloadChangeType type);
const char* rogue_reload_priority_to_string(RogueReloadPriority priority);
const char* rogue_reload_stage_to_string(RogueReloadStage stage);

/* Statistics */
void rogue_hot_reload_get_statistics(const RogueHotReloadSystem* system, uint64_t* files_watched, uint64_t* changes_detected, uint64_t* reloads_successful, uint64_t* reloads_failed, uint64_t* rollbacks_performed);
void rogue_hot_reload_print_statistics(const RogueHotReloadSystem* system);

/* Configuration */
bool rogue_hot_reload_load_config(RogueHotReloadSystem* system, const char* config_file);
bool rogue_hot_reload_save_config(const RogueHotReloadSystem* system, const char* config_file);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_HOT_RELOAD_H */
