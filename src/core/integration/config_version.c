/**
 * @file config_version.c
 * @brief Configuration version management and event type registry system
 *
 * This module provides comprehensive configuration version management and event type
 * registration services for the Roguelike game engine. It handles:
 *
 * - Configuration schema versioning and migration support
 * - Event type ID registration and collision detection
 * - Reserved ID range management for different subsystems
 * - Configuration file validation and integrity checking
 * - Cross-platform directory and file operations
 *
 * The system supports automatic migration detection, backup creation, and strict
 * validation of configuration files. Event type IDs are managed through a registry
 * system that prevents collisions and supports reserved ranges for different game
 * subsystems.
 *
 * @note This implementation supports expanded event type limits (4096 vs 512) and
 *       includes comprehensive logging for debugging and monitoring.
 *
 * @author Configuration Management Team
 * @version 1.0.0
 * @date 2024
 */

#include "config_version.h"
#include "../../util/log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _MSC_VER
#include <direct.h>
#include <io.h>
#define access _access
#define mkdir _mkdir
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/* Global configuration manager instance */
static RogueConfigManager g_config_manager = {0};
static bool g_config_manager_initialized = false;

/* Event type registry for collision detection */
static RogueEventTypeRegistration g_event_type_registry[4096]; /* Expanded from 512 */
static uint32_t g_event_type_count = 0;
static bool g_event_type_registry_initialized = false;

/* Reserved event type ID ranges */
typedef struct
{
    uint32_t start_id;
    uint32_t end_id;
    char system_name[64];
    time_t reservation_time;
} RogueEventTypeReservedRange;

static RogueEventTypeReservedRange g_reserved_ranges[32];
static uint32_t g_reserved_range_count = 0;

/* Forward declarations */
static bool create_directory_if_not_exists(const char* path);
static bool file_exists(const char* path);
static uint32_t calculate_string_hash(const char* str);
static bool validate_event_type_name(const char* name);
static void initialize_default_reserved_ranges(void);

/* ===== Configuration Version Management Implementation ===== */

/**
 * @brief Initializes the configuration version management system
 *
 * This function sets up the configuration manager with the specified directory
 * for storing configuration files and backups. It initializes the current schema
 * version, creates necessary directories, and prepares the event type registry
 * for operation.
 *
 * @param config_directory Path to the directory where configuration files will be stored
 *
 * @return bool Returns true on successful initialization, false on failure
 *
 * @note This function is idempotent - calling it multiple times will only
 *       initialize once and return true on subsequent calls.
 *
 * @warning The function will create the config directory and a backups subdirectory
 *          if they don't exist. Ensure the process has appropriate permissions.
 *
 * @see rogue_config_version_shutdown() for cleanup
 */
bool rogue_config_version_init(const char* config_directory)
{
    if (g_config_manager_initialized)
    {
        ROGUE_LOG_WARN("Configuration version manager already initialized");
        return true;
    }

    if (!config_directory)
    {
        ROGUE_LOG_ERROR("Configuration directory path is NULL");
        return false;
    }

    /* Clear manager state */
    memset(&g_config_manager, 0, sizeof(RogueConfigManager));

    /* Set up directories */
#ifdef _MSC_VER
    strncpy_s(g_config_manager.config_directory, sizeof(g_config_manager.config_directory),
              config_directory, _TRUNCATE);
    snprintf(g_config_manager.backup_directory, sizeof(g_config_manager.backup_directory),
             "%s\\backups", config_directory);
#else
    strncpy(g_config_manager.config_directory, config_directory,
            sizeof(g_config_manager.config_directory) - 1);
    g_config_manager.config_directory[sizeof(g_config_manager.config_directory) - 1] = '\0';
    snprintf(g_config_manager.backup_directory, sizeof(g_config_manager.backup_directory),
             "%s/backups", config_directory);
#endif

    /* Create directories if they don't exist */
    if (!create_directory_if_not_exists(g_config_manager.config_directory))
    {
        ROGUE_LOG_ERROR("Failed to create config directory: %s", g_config_manager.config_directory);
        return false;
    }

    if (!create_directory_if_not_exists(g_config_manager.backup_directory))
    {
        ROGUE_LOG_ERROR("Failed to create backup directory: %s", g_config_manager.backup_directory);
        return false;
    }

    /* Initialize current schema */
    g_config_manager.current_schema.version.major = ROGUE_CONFIG_VERSION_MAJOR;
    g_config_manager.current_schema.version.minor = ROGUE_CONFIG_VERSION_MINOR;
    g_config_manager.current_schema.version.patch = ROGUE_CONFIG_VERSION_PATCH;
    g_config_manager.current_schema.version.created_timestamp = time(NULL);
    g_config_manager.current_schema.version.schema_hash = 0; /* Will be calculated */

#ifdef _MSC_VER
    strncpy_s(g_config_manager.current_schema.version.schema_name,
              sizeof(g_config_manager.current_schema.version.schema_name),
              "Roguelike Integration Plumbing", _TRUNCATE);
#else
    strncpy(g_config_manager.current_schema.version.schema_name, "Roguelike Integration Plumbing",
            sizeof(g_config_manager.current_schema.version.schema_name) - 1);
    g_config_manager.current_schema.version
        .schema_name[sizeof(g_config_manager.current_schema.version.schema_name) - 1] = '\0';
#endif

    /* Set default configuration */
    g_config_manager.auto_migrate_enabled = true;
    g_config_manager.backup_before_migration = true;
    g_config_manager.current_schema.strict_validation_enabled = true;
    g_config_manager.current_schema.max_event_types = 4096; /* Expanded limit */

    /* Initialize event type registry */
    ROGUE_LOG_DEBUG("Init: registry_initialized=%s count=%u reserved_count=%u (pre)",
                    g_event_type_registry_initialized ? "true" : "false", g_event_type_count,
                    g_reserved_range_count);
    if (!g_event_type_registry_initialized)
    {
        memset(g_event_type_registry, 0, sizeof(g_event_type_registry));
        g_event_type_count = 0;
        initialize_default_reserved_ranges();
        g_event_type_registry_initialized = true;
        ROGUE_LOG_DEBUG("Init: registry reset complete; registry_initialized=%s count=%u "
                        "reserved_count=%u (post)",
                        g_event_type_registry_initialized ? "true" : "false", g_event_type_count,
                        g_reserved_range_count);
    }

    g_config_manager_initialized = true;

    ROGUE_LOG_INFO("Configuration version manager initialized (Version: %u.%u.%u)",
                   ROGUE_CONFIG_VERSION_MAJOR, ROGUE_CONFIG_VERSION_MINOR,
                   ROGUE_CONFIG_VERSION_PATCH);

    return true;
}

/**
 * @brief Shuts down the configuration version management system
 *
 * This function performs cleanup of all allocated resources, frees memory,
 * and resets the configuration manager to its initial state. It also cleans
 * up the event type registry to ensure clean state for subsequent initializations.
 *
 * @note This function is safe to call even if the system is not initialized.
 *       It will simply return without performing any operations.
 *
 * @see rogue_config_version_init() for initialization
 */
void rogue_config_version_shutdown(void)
{
    if (!g_config_manager_initialized)
    {
        return;
    }

    /* Free allocated migrations */
    if (g_config_manager.migrations)
    {
        free(g_config_manager.migrations);
        g_config_manager.migrations = NULL;
    }

    /* Free allocated validation rules */
    if (g_config_manager.current_schema.rules)
    {
        free(g_config_manager.current_schema.rules);
        g_config_manager.current_schema.rules = NULL;
    }

    /* Clear state */
    memset(&g_config_manager, 0, sizeof(RogueConfigManager));
    g_config_manager_initialized = false;

    /* Also reset the global event type registry and reservations so each
       initialization starts from a clean state. This ensures tests that
       rely on isolated runs (e.g., rapid registration in Phase 2.8.7)
       don't collide with IDs registered by earlier tests. */
    ROGUE_LOG_DEBUG("Shutdown: registry_initialized=%s count=%u reserved_count=%u (pre)",
                    g_event_type_registry_initialized ? "true" : "false", g_event_type_count,
                    g_reserved_range_count);
    if (g_event_type_registry_initialized)
    {
        memset(g_event_type_registry, 0, sizeof(g_event_type_registry));
        g_event_type_count = 0;
        memset(g_reserved_ranges, 0, sizeof(g_reserved_ranges));
        g_reserved_range_count = 0;
        g_event_type_registry_initialized = false;
        ROGUE_LOG_DEBUG("Shutdown: registry reset complete; registry_initialized=%s count=%u "
                        "reserved_count=%u (post)",
                        g_event_type_registry_initialized ? "true" : "false", g_event_type_count,
                        g_reserved_range_count);
    }

    ROGUE_LOG_INFO("Configuration version manager shutdown complete");
}

/**
 * @brief Retrieves the current configuration schema version
 *
 * This function returns a pointer to the current configuration version information,
 * including major, minor, and patch version numbers, schema name, and creation timestamp.
 *
 * @return const RogueConfigVersion* Pointer to the current version structure, or NULL if not
 * initialized
 *
 * @note The returned pointer points to internal data and should not be modified.
 *       The data remains valid until rogue_config_version_shutdown() is called.
 *
 * @see rogue_config_version_compare() for version comparison utilities
 */
const RogueConfigVersion* rogue_config_get_current_version(void)
{
    if (!g_config_manager_initialized)
    {
        return NULL;
    }

    return &g_config_manager.current_schema.version;
}

/**
 * @brief Compares two configuration versions for ordering
 *
 * This function performs a lexicographic comparison of two version structures,
 * comparing major, minor, and patch versions in order. It follows semantic
 * versioning rules where higher version numbers indicate newer versions.
 *
 * @param a Pointer to the first version structure to compare
 * @param b Pointer to the second version structure to compare
 *
 * @return int Returns:
 *         - 1 if version 'a' is greater than version 'b'
 *         - -1 if version 'a' is less than version 'b'
 *         - 0 if versions are equal
 *
 * @note NULL pointers will result in a return value of 0 (equal).
 *       Comparison follows semantic versioning: major.minor.patch
 *
 * @see rogue_config_get_current_version() for obtaining version structures
 */
int rogue_config_version_compare(const RogueConfigVersion* a, const RogueConfigVersion* b)
{
    if (!a || !b)
    {
        return 0;
    }

    if (a->major != b->major)
    {
        return (a->major > b->major) ? 1 : -1;
    }

    if (a->minor != b->minor)
    {
        return (a->minor > b->minor) ? 1 : -1;
    }

    if (a->patch != b->patch)
    {
        return (a->patch > b->patch) ? 1 : -1;
    }

    return 0; /* Versions are equal */
}

/**
 * @brief Determines if a configuration file needs migration
 *
 * This function analyzes a configuration file to detect if it requires migration
 * to the current schema version. It checks the file's version against the current
 * system version and determines if migration is necessary.
 *
 * @param config_file_path Path to the configuration file to check
 * @param detected_version Pointer to structure that will receive the detected file version
 *
 * @return bool Returns true if migration is needed, false otherwise
 *
 * @note This is currently a stub implementation that assumes legacy version 0.9.0
 *       for all files. A full implementation would parse the file to detect its
 *       actual version.
 *
 * @warning The detected_version structure must be provided and will be overwritten.
 *
 * @see rogue_config_version_compare() for version comparison
 */
bool rogue_config_needs_migration(const char* config_file_path,
                                  RogueConfigVersion* detected_version)
{
    if (!config_file_path || !detected_version)
    {
        return false;
    }

    /* For now, this is a stub implementation */
    /* In a full implementation, this would parse the file and detect its version */

    /* Initialize detected version to a default old version */
    detected_version->major = 0;
    detected_version->minor = 9;
    detected_version->patch = 0;
    detected_version->schema_hash = 0;
    detected_version->created_timestamp = 0;
#ifdef _MSC_VER
    strncpy_s(detected_version->schema_name, sizeof(detected_version->schema_name),
              "Legacy Configuration", _TRUNCATE);
#else
    strncpy(detected_version->schema_name, "Legacy Configuration",
            sizeof(detected_version->schema_name) - 1);
    detected_version->schema_name[sizeof(detected_version->schema_name) - 1] = '\0';
#endif

    /* Check if file exists */
    if (!file_exists(config_file_path))
    {
        return false; /* Non-existent files don't need migration */
    }

    /* Compare with current version */
    const RogueConfigVersion* current = rogue_config_get_current_version();
    if (current)
    {
        return rogue_config_version_compare(detected_version, current) < 0;
    }

    return false;
}

/* ===== Event Type ID Management Implementation ===== */

/**
 * @brief Safely registers a new event type with collision detection
 *
 * This function registers a new event type ID with the global registry, performing
 * comprehensive validation and collision detection. It ensures that event IDs
 * are unique and properly tracked for debugging and monitoring purposes.
 *
 * @param event_id The unique identifier for the event type (must be > 0)
 * @param name Human-readable name for the event type (alphanumeric and underscore only)
 * @param source_file Source file where the registration is occurring (for debugging)
 * @param line_number Line number where the registration is occurring (for debugging)
 *
 * @return bool Returns true on successful registration, false on failure
 *
 * @note Event type names must be 1-63 characters long, start with a letter or
 *       underscore, and contain only alphanumeric characters and underscores.
 *
 * @warning Duplicate event ID registrations will fail. Use rogue_event_type_check_collision()
 *          to check for conflicts before registration.
 *
 * @see rogue_event_type_check_collision() for collision detection
 * @see rogue_event_type_validate_id() for ID validation
 */
bool rogue_event_type_register_safe(uint32_t event_id, const char* name, const char* source_file,
                                    uint32_t line_number)
{
    ROGUE_LOG_DEBUG("RegisterSafe: entering with id=%u name='%s' registry_initialized=%s count=%u",
                    event_id, name ? name : "(null)",
                    g_event_type_registry_initialized ? "true" : "false", g_event_type_count);
    if (!g_event_type_registry_initialized && !rogue_config_version_init("./config"))
    {
        ROGUE_LOG_ERROR("Failed to initialize configuration manager for event type registration");
        return false;
    }

    if (!name || strlen(name) == 0)
    {
        ROGUE_LOG_ERROR("Event type name is NULL or empty");
        return false;
    }

    if (!validate_event_type_name(name))
    {
        ROGUE_LOG_ERROR("Invalid event type name: %s", name);
        return false;
    }

    /* Validate event type ID */
    char error_msg[256];
    if (!rogue_event_type_validate_id(event_id, error_msg, sizeof(error_msg)))
    {
        ROGUE_LOG_ERROR("Invalid event type ID %u: %s", event_id, error_msg);
        return false;
    }

    /* Check for collision: if the ID is already registered, reject duplicate
       registrations. Tests expect duplicate event IDs to fail instead of being
       treated as idempotent re-registers. */
    for (uint32_t i = 0; i < g_event_type_count; i++)
    {
        if (g_event_type_registry[i].event_id == event_id)
        {
            ROGUE_LOG_ERROR(
                "Event type ID %u already registered as '%s' (attempted re-register from %s:%u)",
                event_id, g_event_type_registry[i].name, source_file ? source_file : "unknown",
                line_number);
            return false;
        }
    }

    /* Check if we have room for more event types */
    if (g_event_type_count >= sizeof(g_event_type_registry) / sizeof(g_event_type_registry[0]))
    {
        ROGUE_LOG_ERROR("Event type registry is full (%u types registered)", g_event_type_count);
        return false;
    }

    /* Register the event type */
    RogueEventTypeRegistration* registration = &g_event_type_registry[g_event_type_count];
    registration->event_id = event_id;
    registration->registration_time = time(NULL);
    registration->is_reserved = false;
    registration->line_number = line_number;

#ifdef _MSC_VER
    strncpy_s(registration->name, sizeof(registration->name), name, _TRUNCATE);
    if (source_file)
    {
        strncpy_s(registration->source_file, sizeof(registration->source_file), source_file,
                  _TRUNCATE);
    }
#else
    strncpy(registration->name, name, sizeof(registration->name) - 1);
    registration->name[sizeof(registration->name) - 1] = '\0';
    if (source_file)
    {
        strncpy(registration->source_file, source_file, sizeof(registration->source_file) - 1);
        registration->source_file[sizeof(registration->source_file) - 1] = '\0';
    }
#endif

    g_event_type_count++;

    ROGUE_LOG_DEBUG("Registered event type %u ('%s') from %s:%u", event_id, name,
                    source_file ? source_file : "unknown", line_number);

    return true;
}

/**
 * @brief Checks for event type ID collisions
 *
 * This function checks if a given event ID conflicts with existing registrations
 * or reserved ranges. It provides detailed collision information for debugging
 * and error reporting purposes.
 *
 * @param event_id The event ID to check for collisions
 * @param collision_info Buffer to receive collision details (if any)
 * @param info_size Size of the collision_info buffer
 *
 * @return bool Returns true if a collision is detected, false if the ID is available
 *
 * @note The collision_info buffer will contain a descriptive message about the
 *       collision if one is found. The buffer is null-terminated.
 *
 * @see rogue_event_type_register_safe() for registration with collision detection
 */
bool rogue_event_type_check_collision(uint32_t event_id, char* collision_info, size_t info_size)
{
    if (!collision_info || info_size == 0)
    {
        return false;
    }

    collision_info[0] = '\0'; /* Initialize as empty string */

    /* Check existing registrations */
    for (uint32_t i = 0; i < g_event_type_count; i++)
    {
        if (g_event_type_registry[i].event_id == event_id)
        {
            snprintf(collision_info, info_size, "ID %u already registered as '%s' from %s:%u",
                     event_id, g_event_type_registry[i].name, g_event_type_registry[i].source_file,
                     g_event_type_registry[i].line_number);
            return true;
        }
    }

    /* Check reserved ranges */
    for (uint32_t i = 0; i < g_reserved_range_count; i++)
    {
        if (event_id >= g_reserved_ranges[i].start_id && event_id <= g_reserved_ranges[i].end_id)
        {
            snprintf(collision_info, info_size, "ID %u is in reserved range %u-%u for system '%s'",
                     event_id, g_reserved_ranges[i].start_id, g_reserved_ranges[i].end_id,
                     g_reserved_ranges[i].system_name);
            return true;
        }
    }

    return false; /* No collision detected */
}

/**
 * @brief Finds the next available event type ID in a specified range
 *
 * This function searches for an unused event ID within the specified range,
 * checking for collisions with existing registrations and reserved ranges.
 * It returns the first available ID that passes validation.
 *
 * @param start_range Starting ID of the search range (inclusive)
 * @param end_range Ending ID of the search range (inclusive)
 *
 * @return uint32_t Returns the next available ID, or 0 if no ID is available in the range
 *
 * @note The search is performed sequentially from start_range to end_range.
 *       IDs are validated using rogue_event_type_validate_id().
 *
 * @warning A return value of 0 indicates no available ID was found, not that
 *          ID 0 is available (ID 0 is always invalid).
 *
 * @see rogue_event_type_validate_id() for ID validation rules
 */
uint32_t rogue_event_type_get_next_available_id(uint32_t start_range, uint32_t end_range)
{
    if (start_range >= end_range)
    {
        ROGUE_LOG_ERROR("Invalid range: start_range (%u) >= end_range (%u)", start_range,
                        end_range);
        return 0;
    }

    for (uint32_t id = start_range; id <= end_range; id++)
    {
        char collision_info[256];
        if (!rogue_event_type_check_collision(id, collision_info, sizeof(collision_info)))
        {
            char error_msg[256];
            if (rogue_event_type_validate_id(id, error_msg, sizeof(error_msg)))
            {
                return id; /* Found available ID */
            }
        }
    }

    ROGUE_LOG_WARN("No available event type ID found in range %u-%u", start_range, end_range);
    return 0; /* No available ID found */
}

/**
 * @brief Reserves a range of event type IDs for a specific system
 *
 * This function reserves a contiguous range of event IDs for exclusive use by
 * a particular system or subsystem. Reserved ranges prevent accidental collisions
 * and provide organized ID allocation for different parts of the game engine.
 *
 * @param start_id Starting ID of the range to reserve (inclusive)
 * @param end_id Ending ID of the range to reserve (inclusive)
 * @param system_name Human-readable name identifying the reserving system
 *
 * @return bool Returns true on successful reservation, false on failure
 *
 * @note Reserved ranges cannot overlap with existing reservations.
 *       The system name is used for logging and debugging purposes.
 *
 * @warning Attempting to register an event ID within a reserved range that
 *          doesn't belong to the reserving system may cause validation failures.
 *
 * @see rogue_event_type_register_safe() for registration within reserved ranges
 */
bool rogue_event_type_reserve_range(uint32_t start_id, uint32_t end_id, const char* system_name)
{
    if (start_id >= end_id)
    {
        ROGUE_LOG_ERROR("Invalid reservation range: %u >= %u", start_id, end_id);
        return false;
    }

    if (!system_name || strlen(system_name) == 0)
    {
        ROGUE_LOG_ERROR("System name is NULL or empty");
        return false;
    }

    if (g_reserved_range_count >= sizeof(g_reserved_ranges) / sizeof(g_reserved_ranges[0]))
    {
        ROGUE_LOG_ERROR("Too many reserved ranges (max: %zu)",
                        sizeof(g_reserved_ranges) / sizeof(g_reserved_ranges[0]));
        return false;
    }

    /* Check for overlapping reservations */
    for (uint32_t i = 0; i < g_reserved_range_count; i++)
    {
        if ((start_id >= g_reserved_ranges[i].start_id &&
             start_id <= g_reserved_ranges[i].end_id) ||
            (end_id >= g_reserved_ranges[i].start_id && end_id <= g_reserved_ranges[i].end_id) ||
            (start_id < g_reserved_ranges[i].start_id && end_id > g_reserved_ranges[i].end_id))
        {
            ROGUE_LOG_ERROR("Reservation range %u-%u overlaps with existing range %u-%u (%s)",
                            start_id, end_id, g_reserved_ranges[i].start_id,
                            g_reserved_ranges[i].end_id, g_reserved_ranges[i].system_name);
            return false;
        }
    }

    /* Add reservation */
    RogueEventTypeReservedRange* reservation = &g_reserved_ranges[g_reserved_range_count];
    reservation->start_id = start_id;
    reservation->end_id = end_id;
    reservation->reservation_time = time(NULL);

#ifdef _MSC_VER
    strncpy_s(reservation->system_name, sizeof(reservation->system_name), system_name, _TRUNCATE);
#else
    strncpy(reservation->system_name, system_name, sizeof(reservation->system_name) - 1);
    reservation->system_name[sizeof(reservation->system_name) - 1] = '\0';
#endif

    g_reserved_range_count++;

    ROGUE_LOG_INFO("Reserved event type ID range %u-%u for system '%s'", start_id, end_id,
                   system_name);
    return true;
}

/**
 * @brief Validates an event type ID for correctness and availability
 *
 * This function performs comprehensive validation of an event type ID, checking
 * for reserved values, range limits, and special cases. It provides detailed
 * error messages for validation failures.
 *
 * @param event_id The event ID to validate
 * @param error_msg Buffer to receive error message if validation fails
 * @param error_msg_size Size of the error message buffer
 *
 * @return bool Returns true if the ID is valid, false if validation fails
 *
 * @note Validation checks include:
 *       - ID cannot be 0 (reserved for invalid/uninitialized)
 *       - ID cannot exceed maximum event types limit (unless in reserved range)
 *       - ID cannot be special reserved values (0xFFFFFFFF, 0xDEADBEEF, etc.)
 *
 * @warning The error_msg buffer must be provided and will be null-terminated.
 *          IDs within reserved ranges are allowed even if they exceed normal limits.
 *
 * @see rogue_event_type_reserve_range() for reserving ID ranges
 */
bool rogue_event_type_validate_id(uint32_t event_id, char* error_msg, size_t error_msg_size)
{
    if (!error_msg || error_msg_size == 0)
    {
        return false;
    }

    error_msg[0] = '\0'; /* Initialize as empty */

    /* Check basic range */
    if (event_id == 0)
    {
        snprintf(error_msg, error_msg_size,
                 "Event type ID cannot be 0 (reserved for invalid/uninitialized)");
        return false;
    }

    /* Allow IDs that fall within reserved ranges (e.g., 0x9000-0x9FFF for tests),
       even if they exceed the normal max_event_types soft limit. */
    bool in_reserved_range = false;
    for (uint32_t i = 0; i < g_reserved_range_count; i++)
    {
        if (event_id >= g_reserved_ranges[i].start_id && event_id <= g_reserved_ranges[i].end_id)
        {
            in_reserved_range = true;
            break;
        }
    }

    /* Check against expanded maximum (increased from 512 to 4096) unless reserved */
    if (!in_reserved_range && event_id > g_config_manager.current_schema.max_event_types)
    {
        snprintf(error_msg, error_msg_size, "Event type ID %u exceeds maximum %u", event_id,
                 g_config_manager.current_schema.max_event_types);
        return false;
    }

    /* Check for special reserved values */
    if (event_id == 0xFFFFFFFF)
    {
        snprintf(error_msg, error_msg_size,
                 "Event type ID 0xFFFFFFFF is reserved for internal use");
        return false;
    }

    if (event_id == 0xDEADBEEF || event_id == 0xCAFEBABE)
    {
        snprintf(error_msg, error_msg_size, "Event type ID 0x%X is reserved for debugging",
                 event_id);
        return false;
    }

    return true; /* ID is valid */
}

/* ===== Configuration Validation Implementation ===== */

/**
 * @brief Validates a configuration file for correctness and integrity
 *
 * This function performs basic validation of a configuration file, checking
 * for existence, accessibility, file size limits, and basic structure. It
 * provides detailed error information for validation failures.
 *
 * @param config_file_path Path to the configuration file to validate
 * @param error_details Buffer to receive detailed error information
 * @param error_details_size Size of the error details buffer
 *
 * @return RogueConfigValidationResult Validation result indicating success or specific failure type
 *
 * @note Current implementation performs basic file checks. Full JSON/CFG format
 *       validation is planned for future implementation (marked with TODO).
 *
 * @warning The error_details buffer must be provided and will be null-terminated.
 *          Files larger than 1MB are rejected as potentially corrupted.
 *
 * @see rogue_config_get_validation_result_description() for result descriptions
 */
RogueConfigValidationResult rogue_config_validate_file(const char* config_file_path,
                                                       char* error_details,
                                                       size_t error_details_size)
{
    if (!config_file_path || !error_details || error_details_size == 0)
    {
        return ROGUE_CONFIG_INVALID_TYPE;
    }

    error_details[0] = '\0'; /* Initialize as empty */

    if (!file_exists(config_file_path))
    {
        snprintf(error_details, error_details_size, "Configuration file does not exist: %s",
                 config_file_path);
        return ROGUE_CONFIG_INVALID_VERSION;
    }

    /* Basic file validation */
#ifdef _MSC_VER
    FILE* file;
    errno_t err = fopen_s(&file, config_file_path, "r");
    if (err != 0 || !file)
    {
        char error_buffer[256];
        strerror_s(error_buffer, sizeof(error_buffer), err);
        snprintf(error_details, error_details_size, "Cannot open configuration file: %s (%s)",
                 config_file_path, error_buffer);
        return ROGUE_CONFIG_CORRUPTION_DETECTED;
    }
#else
    FILE* file = fopen(config_file_path, "r");
    if (!file)
    {
        snprintf(error_details, error_details_size, "Cannot open configuration file: %s (%s)",
                 config_file_path, strerror(errno));
        return ROGUE_CONFIG_CORRUPTION_DETECTED;
    }
#endif

    /* Check file size */
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);

    if (file_size <= 0)
    {
        snprintf(error_details, error_details_size, "Configuration file is empty: %s",
                 config_file_path);
        return ROGUE_CONFIG_MISSING_REQUIRED_FIELDS;
    }

    if (file_size > 1024 * 1024)
    { /* 1MB limit */
        snprintf(error_details, error_details_size, "Configuration file too large: %ld bytes",
                 file_size);
        return ROGUE_CONFIG_OUT_OF_RANGE;
    }

    /* TODO: Add JSON/CFG format validation */
    /* For now, we assume the file is valid if it exists and has reasonable size */

    return ROGUE_CONFIG_VALID;
}

/* ===== Utility Function Implementations ===== */

/**
 * @brief Creates a directory if it doesn't already exist
 *
 * This internal utility function checks if a directory exists and creates it
 * if it doesn't. It handles cross-platform differences between Windows and Unix
 * directory creation APIs.
 *
 * @param path Path to the directory to create
 *
 * @return bool Returns true on success (directory exists or was created), false on failure
 *
 * @note On Unix systems, the directory is created with permissions 0755.
 *       On Windows, default permissions are used.
 *
 * @internal This is an internal utility function not exposed in the public API.
 */
static bool create_directory_if_not_exists(const char* path)
{
    if (access(path, 0) == 0)
    {
        return true; /* Directory already exists */
    }

#ifdef _MSC_VER
    return mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

/**
 * @brief Checks if a file or directory exists
 *
 * This internal utility function checks whether a given path exists in the
 * filesystem using the access() system call.
 *
 * @param path Path to check for existence
 *
 * @return bool Returns true if the path exists, false otherwise
 *
 * @internal This is an internal utility function not exposed in the public API.
 */
static bool file_exists(const char* path) { return access(path, 0) == 0; }

/**
 * @brief Calculates a simple hash value for a string
 *
 * This internal utility function implements the djb2 hash algorithm to generate
 * a 32-bit hash value from a null-terminated string. It's used for various
 * internal purposes like schema hashing.
 *
 * @param str Null-terminated string to hash
 *
 * @return uint32_t 32-bit hash value, or 0 if input string is NULL
 *
 * @note This implements the djb2 algorithm: hash = ((hash << 5) + hash) + c
 *       with initial value 5381.
 *
 * @internal This is an internal utility function not exposed in the public API.
 */
static uint32_t calculate_string_hash(const char* str)
{
    if (!str)
        return 0;

    uint32_t hash = 5381;
    int c;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

/**
 * @brief Validates an event type name for correctness
 *
 * This internal utility function validates event type names according to the
 * following rules:
 * - Must not be NULL or empty
 * - Must be less than 64 characters long
 * - Must contain only alphanumeric characters and underscores
 * - Must not start with a digit
 *
 * @param name Event type name to validate
 *
 * @return bool Returns true if the name is valid, false otherwise
 *
 * @internal This is an internal utility function not exposed in the public API.
 */
static bool validate_event_type_name(const char* name)
{
    if (!name || strlen(name) == 0)
    {
        return false;
    }

    size_t len = strlen(name);
    if (len >= 64)
    { /* Name too long */
        return false;
    }

    /* Check for valid characters (alphanumeric and underscore only) */
    for (size_t i = 0; i < len; i++)
    {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_'))
        {
            return false;
        }
    }

    /* Name must not start with a digit */
    if (name[0] >= '0' && name[0] <= '9')
    {
        return false;
    }

    return true;
}

/**
 * @brief Initializes the default reserved event type ID ranges
 *
 * This internal utility function sets up the standard reserved ranges for
 * different subsystems of the game engine. It reserves ranges for core systems,
 * future expansion, testing, and debugging purposes.
 *
 * @note Reserved ranges include:
 *       - 0x0001-0x00FF: Core Entity Events
 *       - 0x0100-0x01FF: Player Action Events
 *       - 0x0200-0x02FF: Combat Events
 *       - 0x0300-0x03FF: Progression Events
 *       - 0x0400-0x04FF: Economy Events
 *       - 0x0500-0x05FF: World Events
 *       - 0x0600-0x06FF: System Events
 *       - 0x9000-0x9FFF: Test Events
 *       - 0xF000-0xFFFF: Debug Events
 *
 * @internal This is an internal utility function not exposed in the public API.
 */
static void initialize_default_reserved_ranges(void)
{
    /* Reserve core system ranges */
    rogue_event_type_reserve_range(0x0001, 0x00FF, "Core Entity Events");
    rogue_event_type_reserve_range(0x0100, 0x01FF, "Player Action Events");
    rogue_event_type_reserve_range(0x0200, 0x02FF, "Combat Events");
    rogue_event_type_reserve_range(0x0300, 0x03FF, "Progression Events");
    rogue_event_type_reserve_range(0x0400, 0x04FF, "Economy Events");
    rogue_event_type_reserve_range(0x0500, 0x05FF, "World Events");
    rogue_event_type_reserve_range(0x0600, 0x06FF, "System Events");

    /* Reserve ranges for future expansion */
    rogue_event_type_reserve_range(0x0700, 0x07FF, "Audio Events");
    rogue_event_type_reserve_range(0x0800, 0x08FF, "Graphics Events");
    rogue_event_type_reserve_range(0x0900, 0x09FF, "Network Events");
    rogue_event_type_reserve_range(0x0A00, 0x0AFF, "AI Events");

    /* Reserve test range (where the problematic 0x9999 event was) */
    rogue_event_type_reserve_range(0x9000, 0x9FFF, "Test Events");

    /* Reserve debugging range */
    rogue_event_type_reserve_range(0xF000, 0xFFFF, "Debug Events");
}

/**
 * @brief Gets a human-readable description of a validation result
 *
 * This function converts a RogueConfigValidationResult enum value into a
 * human-readable string description suitable for logging and user interfaces.
 *
 * @param result The validation result to describe
 *
 * @return const char* Human-readable description of the validation result
 *
 * @note The returned string is a static constant and should not be modified.
 *       Unknown result values return "Unknown validation result".
 *
 * @see RogueConfigValidationResult for the complete list of validation results
 * @see rogue_config_validate_file() for validation function
 */
const char* rogue_config_get_validation_result_description(RogueConfigValidationResult result)
{
    switch (result)
    {
    case ROGUE_CONFIG_VALID:
        return "Configuration is valid";
    case ROGUE_CONFIG_INVALID_VERSION:
        return "Invalid or unsupported configuration version";
    case ROGUE_CONFIG_MISSING_REQUIRED_FIELDS:
        return "Required configuration fields are missing";
    case ROGUE_CONFIG_INVALID_TYPE:
        return "Configuration field has invalid type";
    case ROGUE_CONFIG_OUT_OF_RANGE:
        return "Configuration value is out of acceptable range";
    case ROGUE_CONFIG_DUPLICATE_ID:
        return "Duplicate identifier detected in configuration";
    case ROGUE_CONFIG_CIRCULAR_DEPENDENCY:
        return "Circular dependency detected in configuration";
    case ROGUE_CONFIG_MIGRATION_REQUIRED:
        return "Configuration migration is required";
    case ROGUE_CONFIG_CORRUPTION_DETECTED:
        return "Configuration file corruption detected";
    case ROGUE_CONFIG_SCHEMA_MISMATCH:
        return "Configuration schema mismatch";
    default:
        return "Unknown validation result";
    }
}
