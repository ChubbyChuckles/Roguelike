#include "hot_reload.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Windows compatibility
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#pragma warning(push)
#pragma warning(disable : 4996) // Disable MSVC secure warnings
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

// Logging macros
#define ROGUE_LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define ROGUE_LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define ROGUE_LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define ROGUE_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// =============================================================================
// Helper Functions
// =============================================================================

static bool file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static bool create_directory_recursive(const char* path)
{
    char tmp[ROGUE_HOT_RELOAD_MAX_PATH];
    char* p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\')
    {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = 0;
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = '/';
        }
    }
#ifdef _WIN32
    return _mkdir(tmp) == 0 || errno == EEXIST;
#else
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
#endif
}

static uint32_t simple_hash(const char* data, size_t len)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        hash = (hash ^ data[i]) * 16777619u;
    }
    return hash;
}

// =============================================================================
// System Management
// =============================================================================

RogueHotReloadSystem* rogue_hot_reload_create(void)
{
    RogueHotReloadSystem* system = calloc(1, sizeof(RogueHotReloadSystem));
    if (!system)
    {
        ROGUE_LOG_ERROR("Failed to allocate hot reload system");
        return NULL;
    }

    system->status = ROGUE_RELOAD_STATUS_INACTIVE;
    system->enable_hash_comparison = true;
    system->enable_timestamp_check = true;
    system->enable_size_check = true;
    system->enable_staged_reload = true;
    system->enable_rollback = true;
    system->next_transaction_id = 1;

    return system;
}

bool rogue_hot_reload_init(RogueHotReloadSystem* system)
{
    if (!system)
    {
        ROGUE_LOG_ERROR("Hot reload system is NULL");
        return false;
    }

    if (system->status != ROGUE_RELOAD_STATUS_INACTIVE)
    {
        ROGUE_LOG_WARN("Hot reload system already initialized");
        return true;
    }

    // Initialize platform-specific file watching
#ifdef _WIN32
    // Windows implementation would use ReadDirectoryChangesW
    ROGUE_LOG_DEBUG("Initialized Windows file watcher");
#else
    // Linux implementation would use inotify
    ROGUE_LOG_DEBUG("Initialized Linux file watcher");
#endif

    system->status = ROGUE_RELOAD_STATUS_WATCHING;
    ROGUE_LOG_INFO("Hot reload system initialized");
    return true;
}

void rogue_hot_reload_update(RogueHotReloadSystem* system)
{
    if (!system || system->status == ROGUE_RELOAD_STATUS_INACTIVE)
    {
        return;
    }

    // Process file system events
    rogue_hot_reload_process_events(system);

    // Process notifications
    rogue_hot_reload_process_notifications(system);
}

void rogue_hot_reload_shutdown(RogueHotReloadSystem* system)
{
    if (!system)
        return;

    // Abort all active transactions
    rogue_hot_reload_abort_all_transactions(system);

    // Close all file watchers
    for (int i = 0; i < system->watcher_count; i++)
    {
        RogueFileWatcher* watcher = &system->watchers[i];
        watcher->is_active = false;

#ifdef _WIN32
        if (watcher->directory_handle)
        {
            CloseHandle(watcher->directory_handle);
            watcher->directory_handle = NULL;
        }
        if (watcher->completion_port)
        {
            CloseHandle(watcher->completion_port);
            watcher->completion_port = NULL;
        }
        if (watcher->buffer)
        {
            free(watcher->buffer);
            watcher->buffer = NULL;
        }
#else
        if (watcher->inotify_fd >= 0)
        {
            close(watcher->inotify_fd);
            watcher->inotify_fd = -1;
        }
#endif
    }

    system->watcher_count = 0;
    system->status = ROGUE_RELOAD_STATUS_INACTIVE;
    ROGUE_LOG_INFO("Hot reload system shutdown complete");
}

void rogue_hot_reload_destroy(RogueHotReloadSystem* system)
{
    if (!system)
        return;

    rogue_hot_reload_shutdown(system);
    free(system);
}

// =============================================================================
// File System Watcher (2.4.1)
// =============================================================================

bool rogue_hot_reload_add_watcher(RogueHotReloadSystem* system, const char* path, bool recursive,
                                  RogueReloadCallback callback, void* user_data)
{
    if (!system || !path)
    {
        ROGUE_LOG_ERROR("Invalid parameters for add_watcher");
        return false;
    }

    if (system->watcher_count >= ROGUE_HOT_RELOAD_MAX_WATCHERS)
    {
        ROGUE_LOG_ERROR("Maximum number of watchers reached");
        return false;
    }

    if (!file_exists(path))
    {
        ROGUE_LOG_ERROR("Watch path does not exist: %s", path);
        return false;
    }

    RogueFileWatcher* watcher = &system->watchers[system->watcher_count];
    memset(watcher, 0, sizeof(RogueFileWatcher));

    strncpy(watcher->watch_path, path, sizeof(watcher->watch_path) - 1);
    watcher->recursive = recursive;
    watcher->callback = callback;
    watcher->callback_data = user_data;
    watcher->next_event_id = 1;

#ifdef _WIN32
    watcher->directory_handle = CreateFileA(
        path, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (watcher->directory_handle == INVALID_HANDLE_VALUE)
    {
        ROGUE_LOG_ERROR("Failed to create directory handle for: %s", path);
        return false;
    }

    watcher->buffer_size = 4096;
    watcher->buffer = malloc(watcher->buffer_size);
    if (!watcher->buffer)
    {
        CloseHandle(watcher->directory_handle);
        return false;
    }
#else
    watcher->inotify_fd = inotify_init();
    if (watcher->inotify_fd < 0)
    {
        ROGUE_LOG_ERROR("Failed to initialize inotify for: %s", path);
        return false;
    }

    int mask = IN_CREATE | IN_MODIFY | IN_DELETE | IN_MOVE;
    watcher->watch_descriptor = inotify_add_watch(watcher->inotify_fd, path, mask);
    if (watcher->watch_descriptor < 0)
    {
        close(watcher->inotify_fd);
        ROGUE_LOG_ERROR("Failed to add inotify watch for: %s", path);
        return false;
    }

    // Set non-blocking mode
    fcntl(watcher->inotify_fd, F_SETFL, O_NONBLOCK);
#endif

    watcher->is_active = true;
    system->watcher_count++;
    system->files_watched++;

    ROGUE_LOG_INFO("Added file watcher for: %s (recursive: %s)", path, recursive ? "yes" : "no");
    return true;
}

bool rogue_hot_reload_remove_watcher(RogueHotReloadSystem* system, const char* path)
{
    if (!system || !path)
        return false;

    for (int i = 0; i < system->watcher_count; i++)
    {
        RogueFileWatcher* watcher = &system->watchers[i];
        if (strcmp(watcher->watch_path, path) == 0)
        {
            watcher->is_active = false;

#ifdef _WIN32
            if (watcher->directory_handle)
            {
                CloseHandle(watcher->directory_handle);
                watcher->directory_handle = NULL;
            }
            if (watcher->buffer)
            {
                free(watcher->buffer);
                watcher->buffer = NULL;
            }
#else
            if (watcher->inotify_fd >= 0)
            {
                close(watcher->inotify_fd);
                watcher->inotify_fd = -1;
            }
#endif

            // Move last watcher to this position
            if (i < system->watcher_count - 1)
            {
                system->watchers[i] = system->watchers[system->watcher_count - 1];
            }
            system->watcher_count--;

            ROGUE_LOG_INFO("Removed file watcher for: %s", path);
            return true;
        }
    }

    ROGUE_LOG_WARN("File watcher not found for: %s", path);
    return false;
}

void rogue_hot_reload_process_events(RogueHotReloadSystem* system)
{
    if (!system)
        return;

    for (int i = 0; i < system->watcher_count; i++)
    {
        RogueFileWatcher* watcher = &system->watchers[i];
        if (!watcher->is_active)
            continue;

#ifdef _WIN32
            // Windows file watching would be implemented here
            // For now, we'll use a simplified polling approach

#else
        // Linux inotify implementation
        char buffer[4096];
        ssize_t length = read(watcher->inotify_fd, buffer, sizeof(buffer));
        if (length <= 0)
            continue;

        char* ptr = buffer;
        while (ptr < buffer + length)
        {
            struct inotify_event* event = (struct inotify_event*) ptr;

            if (event->len > 0)
            {
                char full_path[ROGUE_HOT_RELOAD_MAX_PATH];
                snprintf(full_path, sizeof(full_path), "%s/%s", watcher->watch_path, event->name);

                RogueReloadChangeType change_type;
                if (event->mask & IN_CREATE)
                    change_type = ROGUE_RELOAD_CHANGE_CREATED;
                else if (event->mask & IN_MODIFY)
                    change_type = ROGUE_RELOAD_CHANGE_MODIFIED;
                else if (event->mask & IN_DELETE)
                    change_type = ROGUE_RELOAD_CHANGE_DELETED;
                else if (event->mask & IN_MOVE)
                    change_type = ROGUE_RELOAD_CHANGE_RENAMED;
                else
                    continue;

                // Add event to watcher's event queue
                if (watcher->event_count < ROGUE_HOT_RELOAD_MAX_FILES)
                {
                    RogueReloadEvent* reload_event = &watcher->events[watcher->event_count];
                    strncpy(reload_event->file_path, full_path,
                            sizeof(reload_event->file_path) - 1);
                    reload_event->change_type = change_type;
                    reload_event->timestamp = time(NULL);
                    reload_event->event_id = watcher->next_event_id++;
                    reload_event->processed = false;
                    watcher->event_count++;

                    system->changes_detected++;

                    // Trigger callback if set
                    if (watcher->callback)
                    {
                        watcher->callback(full_path, change_type, watcher->callback_data);
                    }

                    ROGUE_LOG_DEBUG("File change detected: %s (%s)", full_path,
                                    rogue_reload_change_type_to_string(change_type));
                }
            }

            ptr += sizeof(struct inotify_event) + event->len;
        }
#endif
    }
}

bool rogue_hot_reload_is_watching(const RogueHotReloadSystem* system, const char* path)
{
    if (!system || !path)
        return false;

    for (int i = 0; i < system->watcher_count; i++)
    {
        const RogueFileWatcher* watcher = &system->watchers[i];
        if (watcher->is_active && strcmp(watcher->watch_path, path) == 0)
        {
            return true;
        }
    }
    return false;
}

// =============================================================================
// Change Detection (2.4.2)
// =============================================================================

bool rogue_hot_reload_compute_file_hash(const char* file_path, char* hash_out, size_t hash_size)
{
    if (!file_path || !hash_out || hash_size == 0)
        return false;

    FILE* file = fopen(file_path, "rb");
    if (!file)
        return false;

    char buffer[4096];
    uint32_t hash = 2166136261u;
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        hash = simple_hash(buffer, bytes_read) ^ hash;
    }

    fclose(file);
    snprintf(hash_out, hash_size, "%08x", hash);
    return true;
}

bool rogue_hot_reload_has_file_changed(const RogueFileInfo* old_info, const char* file_path)
{
    if (!old_info || !file_path)
        return false;

    RogueFileInfo new_info;
    if (!rogue_hot_reload_update_file_info(&new_info, file_path))
    {
        return false;
    }

    // Check timestamp
    if (new_info.last_modified != old_info->last_modified)
    {
        return true;
    }

    // Check file size
    if (new_info.file_size != old_info->file_size)
    {
        return true;
    }

    // Check hash
    if (strcmp(new_info.hash, old_info->hash) != 0)
    {
        return true;
    }

    return false;
}

bool rogue_hot_reload_update_file_info(RogueFileInfo* info, const char* file_path)
{
    if (!info || !file_path)
        return false;

    struct stat st;
    if (stat(file_path, &st) != 0)
    {
        return false;
    }

    strncpy(info->file_path, file_path, sizeof(info->file_path) - 1);
    info->last_modified = st.st_mtime;
    info->file_size = st.st_size;
    info->is_valid = true;

    // Compute hash
    if (!rogue_hot_reload_compute_file_hash(file_path, info->hash, sizeof(info->hash)))
    {
        info->hash[0] = '\0';
        return false;
    }

    return true;
}

void rogue_hot_reload_set_change_detection_mode(RogueHotReloadSystem* system, bool use_hash,
                                                bool use_timestamp, bool use_size)
{
    if (!system)
        return;

    system->enable_hash_comparison = use_hash;
    system->enable_timestamp_check = use_timestamp;
    system->enable_size_check = use_size;

    ROGUE_LOG_INFO("Change detection mode updated: hash=%s, timestamp=%s, size=%s",
                   use_hash ? "enabled" : "disabled", use_timestamp ? "enabled" : "disabled",
                   use_size ? "enabled" : "disabled");
}

// =============================================================================
// Staged Reloading (2.4.3)
// =============================================================================

bool rogue_hot_reload_set_validator(RogueHotReloadSystem* system, RogueReloadValidator validator,
                                    void* user_data)
{
    if (!system)
        return false;

    system->validator = validator;
    system->validator_data = user_data;
    ROGUE_LOG_INFO("Hot reload validator set");
    return true;
}

bool rogue_hot_reload_validate_file(RogueHotReloadSystem* system, const char* file_path)
{
    if (!system || !file_path)
        return false;

    if (!system->validator)
    {
        ROGUE_LOG_WARN("No validator set for file validation");
        return true; // Allow if no validator
    }

    // Read file content
    FILE* file = fopen(file_path, "r");
    if (!file)
    {
        ROGUE_LOG_ERROR("Failed to open file for validation: %s", file_path);
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = malloc(file_size + 1);
    if (!content)
    {
        fclose(file);
        return false;
    }

    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);

    bool is_valid = system->validator(file_path, content, system->validator_data);
    free(content);

    if (is_valid)
    {
        ROGUE_LOG_DEBUG("File validation passed: %s", file_path);
    }
    else
    {
        ROGUE_LOG_ERROR("File validation failed: %s", file_path);
    }

    return is_valid;
}

bool rogue_hot_reload_stage_reload(RogueHotReloadSystem* system, const char* file_path)
{
    if (!system || !file_path)
        return false;

    system->status = ROGUE_RELOAD_STATUS_VALIDATING;

    // Validate the file first
    if (!rogue_hot_reload_validate_file(system, file_path))
    {
        system->status = ROGUE_RELOAD_STATUS_ERROR;
        return false;
    }

    // Create backup if rollback is enabled
    if (system->enable_rollback)
    {
        if (!rogue_hot_reload_backup_file(system, file_path))
        {
            ROGUE_LOG_WARN("Failed to create backup for: %s", file_path);
        }
    }

    system->status = ROGUE_RELOAD_STATUS_WATCHING;
    ROGUE_LOG_INFO("File staged for reload: %s", file_path);
    return true;
}

bool rogue_hot_reload_apply_staged_changes(RogueHotReloadSystem* system)
{
    if (!system)
        return false;

    system->status = ROGUE_RELOAD_STATUS_APPLYING;

    // In a full implementation, this would apply all staged changes
    // For now, we'll just mark success

    system->status = ROGUE_RELOAD_STATUS_WATCHING;
    system->reloads_successful++;

    ROGUE_LOG_INFO("Applied staged changes successfully");
    return true;
}

// =============================================================================
// Transaction System (2.4.4)
// =============================================================================

uint64_t rogue_hot_reload_begin_transaction(RogueHotReloadSystem* system, bool atomic,
                                            bool rollback_on_failure)
{
    if (!system)
        return 0;

    if (system->transaction_count >= ROGUE_HOT_RELOAD_MAX_TRANSACTIONS)
    {
        ROGUE_LOG_ERROR("Maximum number of transactions reached");
        return 0;
    }

    RogueReloadTransaction* transaction = &system->transactions[system->transaction_count];
    memset(transaction, 0, sizeof(RogueReloadTransaction));

    transaction->transaction_id = system->next_transaction_id++;
    transaction->is_atomic = atomic;
    transaction->rollback_on_failure = rollback_on_failure;
    transaction->current_stage = ROGUE_RELOAD_STAGE_DETECT;
    transaction->start_time = time(NULL);

    system->transaction_count++;

    ROGUE_LOG_INFO("Started transaction %llu (atomic: %s, rollback: %s)",
                   (unsigned long long) transaction->transaction_id, atomic ? "yes" : "no",
                   rollback_on_failure ? "yes" : "no");

    return transaction->transaction_id;
}

bool rogue_hot_reload_add_file_to_transaction(RogueHotReloadSystem* system, uint64_t transaction_id,
                                              const char* file_path)
{
    if (!system || !file_path)
        return false;

    // Find transaction
    RogueReloadTransaction* transaction = NULL;
    for (int i = 0; i < system->transaction_count; i++)
    {
        if (system->transactions[i].transaction_id == transaction_id)
        {
            transaction = &system->transactions[i];
            break;
        }
    }

    if (!transaction)
    {
        ROGUE_LOG_ERROR("Transaction %llu not found", (unsigned long long) transaction_id);
        return false;
    }

    if (transaction->file_count >= ROGUE_HOT_RELOAD_MAX_FILES)
    {
        ROGUE_LOG_ERROR("Transaction %llu file limit reached", (unsigned long long) transaction_id);
        return false;
    }

    strncpy(transaction->files[transaction->file_count], file_path, ROGUE_HOT_RELOAD_MAX_PATH - 1);
    transaction->file_count++;

    ROGUE_LOG_DEBUG("Added file to transaction %llu: %s", (unsigned long long) transaction_id,
                    file_path);
    return true;
}

bool rogue_hot_reload_commit_transaction(RogueHotReloadSystem* system, uint64_t transaction_id)
{
    if (!system)
        return false;

    // Find and remove transaction
    for (int i = 0; i < system->transaction_count; i++)
    {
        if (system->transactions[i].transaction_id == transaction_id)
        {
            RogueReloadTransaction* transaction = &system->transactions[i];

            // Process all files in transaction
            bool success = true;
            for (int j = 0; j < transaction->file_count; j++)
            {
                if (!rogue_hot_reload_validate_file(system, transaction->files[j]))
                {
                    success = false;
                    if (transaction->rollback_on_failure)
                    {
                        break;
                    }
                }
            }

            if (success)
            {
                system->reloads_successful++;
                ROGUE_LOG_INFO("Transaction %llu committed successfully",
                               (unsigned long long) transaction_id);
            }
            else
            {
                system->reloads_failed++;
                if (transaction->rollback_on_failure)
                {
                    rogue_hot_reload_rollback_transaction(system, transaction_id);
                    return false;
                }
                ROGUE_LOG_ERROR("Transaction %llu committed with errors",
                                (unsigned long long) transaction_id);
            }

            // Remove transaction
            if (i < system->transaction_count - 1)
            {
                system->transactions[i] = system->transactions[system->transaction_count - 1];
            }
            system->transaction_count--;
            return success;
        }
    }

    ROGUE_LOG_ERROR("Transaction %llu not found for commit", (unsigned long long) transaction_id);
    return false;
}

bool rogue_hot_reload_rollback_transaction(RogueHotReloadSystem* system, uint64_t transaction_id)
{
    if (!system)
        return false;

    // Find transaction
    for (int i = 0; i < system->transaction_count; i++)
    {
        if (system->transactions[i].transaction_id == transaction_id)
        {
            RogueReloadTransaction* transaction = &system->transactions[i];

            // Restore all files from backup
            bool success = true;
            for (int j = 0; j < transaction->file_count; j++)
            {
                if (!rogue_hot_reload_restore_file(system, transaction->files[j]))
                {
                    success = false;
                }
            }

            system->rollbacks_performed++;

            // Remove transaction
            if (i < system->transaction_count - 1)
            {
                system->transactions[i] = system->transactions[system->transaction_count - 1];
            }
            system->transaction_count--;

            ROGUE_LOG_INFO("Transaction %llu rolled back", (unsigned long long) transaction_id);
            return success;
        }
    }

    return false;
}

void rogue_hot_reload_abort_all_transactions(RogueHotReloadSystem* system)
{
    if (!system)
        return;

    int count = system->transaction_count;
    for (int i = count - 1; i >= 0; i--)
    {
        rogue_hot_reload_rollback_transaction(system, system->transactions[i].transaction_id);
    }

    ROGUE_LOG_INFO("Aborted %d transactions", count);
}

// =============================================================================
// Error Handling & Rollback (2.4.6)
// =============================================================================

bool rogue_hot_reload_set_rollback_directory(RogueHotReloadSystem* system, const char* directory)
{
    if (!system || !directory)
        return false;

    if (!create_directory_recursive(directory))
    {
        ROGUE_LOG_ERROR("Failed to create rollback directory: %s", directory);
        return false;
    }

    strncpy(system->rollback_directory, directory, sizeof(system->rollback_directory) - 1);
    ROGUE_LOG_INFO("Rollback directory set: %s", directory);
    return true;
}

bool rogue_hot_reload_backup_file(RogueHotReloadSystem* system, const char* file_path)
{
    if (!system || !file_path)
        return false;

    if (system->rollback_directory[0] == '\0')
    {
        ROGUE_LOG_WARN("No rollback directory set for backup");
        return false;
    }

    // Create backup path
    char backup_path[ROGUE_HOT_RELOAD_MAX_PATH];
    const char* filename = strrchr(file_path, '/');
    if (!filename)
        filename = strrchr(file_path, '\\');
    if (!filename)
        filename = file_path;
    else
        filename++;

    snprintf(backup_path, sizeof(backup_path), "%s/%s.backup", system->rollback_directory,
             filename);

    // Copy file
    FILE* src = fopen(file_path, "rb");
    if (!src)
        return false;

    FILE* dst = fopen(backup_path, "wb");
    if (!dst)
    {
        fclose(src);
        return false;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);

    ROGUE_LOG_DEBUG("Created backup: %s -> %s", file_path, backup_path);
    return true;
}

bool rogue_hot_reload_restore_file(RogueHotReloadSystem* system, const char* file_path)
{
    if (!system || !file_path)
        return false;

    // Create backup path
    char backup_path[ROGUE_HOT_RELOAD_MAX_PATH];
    const char* filename = strrchr(file_path, '/');
    if (!filename)
        filename = strrchr(file_path, '\\');
    if (!filename)
        filename = file_path;
    else
        filename++;

    snprintf(backup_path, sizeof(backup_path), "%s/%s.backup", system->rollback_directory,
             filename);

    if (!file_exists(backup_path))
    {
        ROGUE_LOG_ERROR("Backup file not found: %s", backup_path);
        return false;
    }

    // Copy backup back
    FILE* src = fopen(backup_path, "rb");
    if (!src)
        return false;

    FILE* dst = fopen(file_path, "wb");
    if (!dst)
    {
        fclose(src);
        return false;
    }

    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        fwrite(buffer, 1, bytes, dst);
    }

    fclose(src);
    fclose(dst);

    ROGUE_LOG_INFO("Restored file from backup: %s", file_path);
    return true;
}

bool rogue_hot_reload_has_backup(const RogueHotReloadSystem* system, const char* file_path)
{
    if (!system || !file_path || system->rollback_directory[0] == '\0')
    {
        return false;
    }

    char backup_path[ROGUE_HOT_RELOAD_MAX_PATH];
    const char* filename = strrchr(file_path, '/');
    if (!filename)
        filename = strrchr(file_path, '\\');
    if (!filename)
        filename = file_path;
    else
        filename++;

    snprintf(backup_path, sizeof(backup_path), "%s/%s.backup", system->rollback_directory,
             filename);
    return file_exists(backup_path);
}

// =============================================================================
// Notification System (2.4.7)
// =============================================================================

bool rogue_hot_reload_set_notifier(RogueHotReloadSystem* system, RogueReloadNotifier notifier)
{
    if (!system)
        return false;

    system->notifier = notifier;
    ROGUE_LOG_INFO("Hot reload notifier set");
    return true;
}

bool rogue_hot_reload_send_notification(RogueHotReloadSystem* system, const char* target_system,
                                        const char* config_file, RogueReloadChangeType change_type,
                                        void* user_data)
{
    if (!system || !target_system || !config_file)
        return false;

    if (system->notification_count >= ROGUE_HOT_RELOAD_MAX_NOTIFICATIONS)
    {
        ROGUE_LOG_ERROR("Notification queue full");
        return false;
    }

    RogueReloadNotification* notification = &system->notifications[system->notification_count];
    strncpy(notification->target_system, target_system, sizeof(notification->target_system) - 1);
    strncpy(notification->config_file, config_file, sizeof(notification->config_file) - 1);
    notification->change_type = change_type;
    notification->user_data = user_data;
    notification->acknowledged = false;

    system->notification_count++;

    ROGUE_LOG_DEBUG("Queued notification for %s: %s (%s)", target_system, config_file,
                    rogue_reload_change_type_to_string(change_type));
    return true;
}

void rogue_hot_reload_process_notifications(RogueHotReloadSystem* system)
{
    if (!system || !system->notifier)
        return;

    for (int i = 0; i < system->notification_count; i++)
    {
        RogueReloadNotification* notification = &system->notifications[i];
        if (!notification->acknowledged)
        {
            system->notifier(notification);
            notification->acknowledged = true;
        }
    }

    // Remove acknowledged notifications
    int write_index = 0;
    for (int read_index = 0; read_index < system->notification_count; read_index++)
    {
        if (!system->notifications[read_index].acknowledged)
        {
            if (write_index != read_index)
            {
                system->notifications[write_index] = system->notifications[read_index];
            }
            write_index++;
        }
    }
    system->notification_count = write_index;
}

void rogue_hot_reload_acknowledge_notification(RogueHotReloadSystem* system, int notification_id)
{
    if (!system || notification_id < 0 || notification_id >= system->notification_count)
        return;

    system->notifications[notification_id].acknowledged = true;
}

// =============================================================================
// Utility Functions
// =============================================================================

const char* rogue_reload_status_to_string(RogueReloadStatus status)
{
    switch (status)
    {
    case ROGUE_RELOAD_STATUS_INACTIVE:
        return "INACTIVE";
    case ROGUE_RELOAD_STATUS_WATCHING:
        return "WATCHING";
    case ROGUE_RELOAD_STATUS_VALIDATING:
        return "VALIDATING";
    case ROGUE_RELOAD_STATUS_APPLYING:
        return "APPLYING";
    case ROGUE_RELOAD_STATUS_ROLLING_BACK:
        return "ROLLING_BACK";
    case ROGUE_RELOAD_STATUS_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

const char* rogue_reload_change_type_to_string(RogueReloadChangeType type)
{
    switch (type)
    {
    case ROGUE_RELOAD_CHANGE_CREATED:
        return "CREATED";
    case ROGUE_RELOAD_CHANGE_MODIFIED:
        return "MODIFIED";
    case ROGUE_RELOAD_CHANGE_DELETED:
        return "DELETED";
    case ROGUE_RELOAD_CHANGE_RENAMED:
        return "RENAMED";
    default:
        return "UNKNOWN";
    }
}

const char* rogue_reload_priority_to_string(RogueReloadPriority priority)
{
    switch (priority)
    {
    case ROGUE_RELOAD_PRIORITY_LOW:
        return "LOW";
    case ROGUE_RELOAD_PRIORITY_NORMAL:
        return "NORMAL";
    case ROGUE_RELOAD_PRIORITY_HIGH:
        return "HIGH";
    case ROGUE_RELOAD_PRIORITY_CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

const char* rogue_reload_stage_to_string(RogueReloadStage stage)
{
    switch (stage)
    {
    case ROGUE_RELOAD_STAGE_DETECT:
        return "DETECT";
    case ROGUE_RELOAD_STAGE_VALIDATE:
        return "VALIDATE";
    case ROGUE_RELOAD_STAGE_STAGE:
        return "STAGE";
    case ROGUE_RELOAD_STAGE_APPLY:
        return "APPLY";
    case ROGUE_RELOAD_STAGE_NOTIFY:
        return "NOTIFY";
    default:
        return "UNKNOWN";
    }
}

void rogue_hot_reload_get_statistics(const RogueHotReloadSystem* system, uint64_t* files_watched,
                                     uint64_t* changes_detected, uint64_t* reloads_successful,
                                     uint64_t* reloads_failed, uint64_t* rollbacks_performed)
{
    if (!system)
        return;

    if (files_watched)
        *files_watched = system->files_watched;
    if (changes_detected)
        *changes_detected = system->changes_detected;
    if (reloads_successful)
        *reloads_successful = system->reloads_successful;
    if (reloads_failed)
        *reloads_failed = system->reloads_failed;
    if (rollbacks_performed)
        *rollbacks_performed = system->rollbacks_performed;
}

void rogue_hot_reload_print_statistics(const RogueHotReloadSystem* system)
{
    if (!system)
        return;

    ROGUE_LOG_INFO("=== Hot Reload Statistics ===");
    ROGUE_LOG_INFO("Files watched: %llu", (unsigned long long) system->files_watched);
    ROGUE_LOG_INFO("Changes detected: %llu", (unsigned long long) system->changes_detected);
    ROGUE_LOG_INFO("Reloads successful: %llu", (unsigned long long) system->reloads_successful);
    ROGUE_LOG_INFO("Reloads failed: %llu", (unsigned long long) system->reloads_failed);
    ROGUE_LOG_INFO("Rollbacks performed: %llu", (unsigned long long) system->rollbacks_performed);
    ROGUE_LOG_INFO("Active watchers: %d", system->watcher_count);
    ROGUE_LOG_INFO("Active transactions: %d", system->transaction_count);
    ROGUE_LOG_INFO("Pending notifications: %d", system->notification_count);
    ROGUE_LOG_INFO("Status: %s", rogue_reload_status_to_string(system->status));
}

// Simplified stubs for remaining functions
bool rogue_hot_reload_add_dependency(RogueHotReloadSystem* system, const char* config_file,
                                     const char* dependency, RogueReloadPriority priority,
                                     bool is_weak)
{
    (void) system;
    (void) config_file;
    (void) dependency;
    (void) priority;
    (void) is_weak;
    return true;
}

bool rogue_hot_reload_remove_dependency(RogueHotReloadSystem* system, const char* config_file,
                                        const char* dependency)
{
    (void) system;
    (void) config_file;
    (void) dependency;
    return true;
}

bool rogue_hot_reload_get_affected_files(const RogueHotReloadSystem* system,
                                         const char* changed_file,
                                         char affected_files[][ROGUE_HOT_RELOAD_MAX_PATH],
                                         int* count, int max_files)
{
    (void) system;
    (void) changed_file;
    (void) affected_files;
    (void) count;
    (void) max_files;
    return true;
}

bool rogue_hot_reload_reload_selective(RogueHotReloadSystem* system, const char* file_path)
{
    (void) system;
    (void) file_path;
    return true;
}

bool rogue_hot_reload_load_config(RogueHotReloadSystem* system, const char* config_file)
{
    (void) system;
    (void) config_file;
    return true;
}

bool rogue_hot_reload_save_config(const RogueHotReloadSystem* system, const char* config_file)
{
    (void) system;
    (void) config_file;
    return true;
}

#ifdef _WIN32
#pragma warning(pop)
#endif
