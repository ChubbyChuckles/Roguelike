/**
 * @file hot_reload.c
 * @brief Hot reload infrastructure for automatic file change detection and reloading.
 * @details This module provides functionality for registering files to monitor for changes,
 * with automatic reloading via content hashing using FNV-1a 64-bit algorithm.
 */

/* hot_reload.c - Phase M3.3 hot reload infrastructure (Complete)
 * Functionality: registration + manual force trigger + automatic change
 * detection via content hash (FNV-1a 64) each tick.
 */
#include "hot_reload.h"
#include <stdio.h>
#include <string.h>

#ifndef ROGUE_HOT_RELOAD_CAP
#define ROGUE_HOT_RELOAD_CAP 64
#endif

/**
 * @brief Entry structure for hot reload monitoring.
 */
typedef struct RogueHotReloadEntry
{
    char id[64];                  /**< Unique identifier for this entry */
    char path[256];               /**< File path to monitor */
    RogueHotReloadFn fn;          /**< Callback function to call on change */
    void* user_data;              /**< User data passed to callback */
    unsigned long long last_hash; /**< Last computed hash (0 means unknown/not yet hashed) */
} RogueHotReloadEntry;

/** @brief Array of hot reload entries */
static RogueHotReloadEntry g_entries[ROGUE_HOT_RELOAD_CAP];
/** @brief Current number of registered entries */
static int g_entry_count = 0;

/**
 * @brief Resets the hot reload system, clearing all registrations.
 * @details Removes all registered entries and resets the system to initial state.
 */
void rogue_hot_reload_reset(void) { g_entry_count = 0; }

/**
 * @brief Finds the index of a registered entry by ID.
 * @param id The unique identifier to search for.
 * @return Index of the entry, or -1 if not found.
 * @details Performs linear search through registered entries.
 */
static int find_index(const char* id)
{
    if (!id)
        return -1;
    for (int i = 0; i < g_entry_count; i++)
    {
        if (strcmp(g_entries[i].id, id) == 0)
            return i;
    }
    return -1;
}

/**
 * @brief Computes FNV-1a 64-bit hash of a file's contents.
 * @param path Path to the file to hash.
 * @return 64-bit hash value, or 0 on error.
 * @details Reads the entire file and computes its hash for change detection.
 */
static unsigned long long hash_file(const char* path)
{
    FILE* f = NULL;
    unsigned long long h = 1469598103934665603ull; /* FNV-1a 64 offset */
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return 0ull;
    unsigned char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0)
    {
        for (size_t i = 0; i < n; i++)
        {
            h ^= (unsigned long long) buf[i];
            h *= 1099511628211ull;
        }
    }
    fclose(f);
    return h;
}

/**
 * @brief Registers a file for hot reload monitoring.
 * @param id Unique identifier for this registration.
 * @param path File path to monitor for changes.
 * @param fn Callback function to call when file changes.
 * @param user_data User data to pass to the callback function.
 * @return 0 on success, -1 on failure.
 * @details Registers a file to be monitored. Duplicate IDs are not allowed.
 * Computes initial hash for change detection.
 */
int rogue_hot_reload_register(const char* id, const char* path, RogueHotReloadFn fn,
                              void* user_data)
{
    if (!id || !*id || !path || !fn)
        return -1;
    if (find_index(id) >= 0)
        return -1; /* duplicate */
    if (g_entry_count >= ROGUE_HOT_RELOAD_CAP)
        return -1;
    RogueHotReloadEntry* e = &g_entries[g_entry_count++];
#if defined(_MSC_VER)
    strncpy_s(e->id, sizeof e->id, id, _TRUNCATE);
    strncpy_s(e->path, sizeof e->path, path, _TRUNCATE);
#else
    strncpy(e->id, id, sizeof e->id - 1);
    e->id[sizeof e->id - 1] = '\0';
    strncpy(e->path, path, sizeof e->path - 1);
    e->path[sizeof e->path - 1] = '\0';
#endif
    e->fn = fn;
    e->user_data = user_data;
    e->last_hash = hash_file(e->path);
    return 0;
}

/**
 * @brief Manually triggers a reload for a registered file.
 * @param id Unique identifier of the file to reload.
 * @return 0 on success, -1 on failure.
 * @details Forces the callback to be called for the specified file,
 * regardless of whether it has actually changed.
 */
int rogue_hot_reload_force(const char* id)
{
    int idx = find_index(id);
    if (idx < 0)
        return -1;
    RogueHotReloadEntry* e = &g_entries[idx];
    e->fn(e->path, e->user_data);
    return 0;
}

/**
 * @brief Checks all registered files for changes and triggers reloads.
 * @return Number of files that were reloaded.
 * @details Called periodically to check for file changes using content hashing.
 * Triggers callbacks for any files that have been modified since last check.
 */
int rogue_hot_reload_tick(void)
{
    int fired = 0;
    for (int i = 0; i < g_entry_count; i++)
    {
        RogueHotReloadEntry* e = &g_entries[i];
        unsigned long long h = hash_file(e->path);
        if (h != 0ull && h != e->last_hash)
        {
            e->last_hash = h;
            e->fn(e->path, e->user_data);
            fired++;
        }
    }
    return fired;
}
