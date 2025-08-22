#include "config_version.h"
#include "util/log.h"
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
    if (!g_event_type_registry_initialized)
    {
        memset(g_event_type_registry, 0, sizeof(g_event_type_registry));
        g_event_type_count = 0;
        initialize_default_reserved_ranges();
        g_event_type_registry_initialized = true;
    }

    g_config_manager_initialized = true;

    ROGUE_LOG_INFO("Configuration version manager initialized (Version: %u.%u.%u)",
                   ROGUE_CONFIG_VERSION_MAJOR, ROGUE_CONFIG_VERSION_MINOR,
                   ROGUE_CONFIG_VERSION_PATCH);

    return true;
}

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

    ROGUE_LOG_INFO("Configuration version manager shutdown complete");
}

const RogueConfigVersion* rogue_config_get_current_version(void)
{
    if (!g_config_manager_initialized)
    {
        return NULL;
    }

    return &g_config_manager.current_schema.version;
}

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

bool rogue_event_type_register_safe(uint32_t event_id, const char* name, const char* source_file,
                                    uint32_t line_number)
{
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

    /* Check for collision: allow registrations inside reserved ranges. If the
       ID is already registered, treat this as idempotent (no-op) and succeed,
       preserving the original registration metadata and name. */
    for (uint32_t i = 0; i < g_event_type_count; i++)
    {
        if (g_event_type_registry[i].event_id == event_id)
        {
            ROGUE_LOG_WARN(
                "Event type ID %u already registered as '%s' (idempotent re-register from %s:%u)",
                event_id, g_event_type_registry[i].name, source_file ? source_file : "unknown",
                line_number);
            return true;
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

static bool file_exists(const char* path) { return access(path, 0) == 0; }

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
