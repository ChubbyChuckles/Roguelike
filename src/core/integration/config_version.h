#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Configuration Version Management (Phase 2.6) */
#define ROGUE_CONFIG_VERSION_MAJOR 1
#define ROGUE_CONFIG_VERSION_MINOR 0
#define ROGUE_CONFIG_VERSION_PATCH 0

/* Maximum configuration schema versions supported */
#define ROGUE_MAX_CONFIG_VERSIONS 64
#define ROGUE_CONFIG_SCHEMA_NAME_MAX 128
#define ROGUE_CONFIG_FILE_PATH_MAX 512

    /* Configuration validation result codes */
    typedef enum
    {
        ROGUE_CONFIG_VALID = 0,
        ROGUE_CONFIG_INVALID_VERSION,
        ROGUE_CONFIG_MISSING_REQUIRED_FIELDS,
        ROGUE_CONFIG_INVALID_TYPE,
        ROGUE_CONFIG_OUT_OF_RANGE,
        ROGUE_CONFIG_DUPLICATE_ID,
        ROGUE_CONFIG_CIRCULAR_DEPENDENCY,
        ROGUE_CONFIG_MIGRATION_REQUIRED,
        ROGUE_CONFIG_CORRUPTION_DETECTED,
        ROGUE_CONFIG_SCHEMA_MISMATCH
    } RogueConfigValidationResult;

    /* Configuration version information */
    typedef struct
    {
        uint16_t major;
        uint16_t minor;
        uint16_t patch;
        uint32_t schema_hash;
        time_t created_timestamp;
        char schema_name[ROGUE_CONFIG_SCHEMA_NAME_MAX];
    } RogueConfigVersion;

    /* Configuration migration info */
    typedef struct
    {
        RogueConfigVersion from_version;
        RogueConfigVersion to_version;
        bool (*migration_func)(const char* old_config, char* new_config, size_t new_config_size);
        char description[256];
    } RogueConfigMigration;

    /* Configuration validation rule */
    typedef struct
    {
        char field_name[64];
        bool required;
        uint32_t min_value;
        uint32_t max_value;
        bool (*custom_validator)(const void* value, char* error_msg, size_t error_msg_size);
    } RogueConfigValidationRule;

    /* Event type ID registry for collision detection */
    typedef struct
    {
        uint32_t event_id;
        char name[64];
        char source_file[256];
        uint32_t line_number;
        bool is_reserved;
        time_t registration_time;
    } RogueEventTypeRegistration;

    /* Configuration schema definition */
    typedef struct
    {
        RogueConfigVersion version;
        RogueConfigValidationRule* rules;
        uint32_t rule_count;
        RogueEventTypeRegistration* event_type_registry;
        uint32_t event_type_count;
        uint32_t max_event_types;
        bool strict_validation_enabled;
    } RogueConfigSchema;

    /* Configuration manager state */
    typedef struct
    {
        RogueConfigSchema current_schema;
        RogueConfigMigration* migrations;
        uint32_t migration_count;
        bool auto_migrate_enabled;
        bool backup_before_migration;
        char config_directory[ROGUE_CONFIG_FILE_PATH_MAX];
        char backup_directory[ROGUE_CONFIG_FILE_PATH_MAX];
    } RogueConfigManager;

    /* ===== Configuration Version Management API ===== */

    /* Initialize configuration version manager */
    bool rogue_config_version_init(const char* config_directory);

    /* Shutdown configuration version manager */
    void rogue_config_version_shutdown(void);

    /* Get current configuration version */
    const RogueConfigVersion* rogue_config_get_current_version(void);

    /* Compare two configuration versions */
    int rogue_config_version_compare(const RogueConfigVersion* a, const RogueConfigVersion* b);

    /* Check if configuration file needs migration */
    bool rogue_config_needs_migration(const char* config_file_path,
                                      RogueConfigVersion* detected_version);

    /* Perform automatic configuration migration */
    RogueConfigValidationResult rogue_config_auto_migrate(const char* config_file_path);

    /* ===== Event Type ID Management ===== */

    /* Register event type with collision detection */
    bool rogue_event_type_register_safe(uint32_t event_id, const char* name,
                                        const char* source_file, uint32_t line_number);

    /* Check for event type ID collisions */
    bool rogue_event_type_check_collision(uint32_t event_id, char* collision_info,
                                          size_t info_size);

    /* Get next available event type ID in range */
    uint32_t rogue_event_type_get_next_available_id(uint32_t start_range, uint32_t end_range);

    /* Reserve event type ID ranges for specific systems */
    bool rogue_event_type_reserve_range(uint32_t start_id, uint32_t end_id,
                                        const char* system_name);

    /* Validate event type ID is within acceptable limits */
    bool rogue_event_type_validate_id(uint32_t event_id, char* error_msg, size_t error_msg_size);

    /* ===== Configuration Validation API ===== */

    /* Validate configuration file structure */
    RogueConfigValidationResult rogue_config_validate_file(const char* config_file_path,
                                                           char* error_details,
                                                           size_t error_details_size);

    /* Validate configuration schema */
    RogueConfigValidationResult rogue_config_validate_schema(const RogueConfigSchema* schema);

    /* Add custom validation rule */
    bool rogue_config_add_validation_rule(const RogueConfigValidationRule* rule);

    /* Enable/disable strict validation mode */
    void rogue_config_set_strict_validation(bool enabled);

    /* ===== Configuration Schema Management ===== */

    /* Create configuration schema from file */
    bool rogue_config_create_schema(const char* schema_file_path, RogueConfigSchema* schema);

    /* Save configuration schema to file */
    bool rogue_config_save_schema(const RogueConfigSchema* schema, const char* schema_file_path);

    /* Update configuration schema version */
    bool rogue_config_update_schema_version(RogueConfigSchema* schema, uint16_t major,
                                            uint16_t minor, uint16_t patch);

    /* Calculate schema hash for integrity checking */
    uint32_t rogue_config_calculate_schema_hash(const RogueConfigSchema* schema);

    /* ===== Configuration Migration API ===== */

    /* Register configuration migration */
    bool rogue_config_register_migration(const RogueConfigMigration* migration);

    /* Execute migration from one version to another */
    RogueConfigValidationResult
    rogue_config_execute_migration(const char* config_file_path,
                                   const RogueConfigVersion* target_version);

    /* Create backup of configuration before migration */
    bool rogue_config_create_backup(const char* config_file_path, char* backup_path,
                                    size_t backup_path_size);

    /* Restore configuration from backup */
    bool rogue_config_restore_backup(const char* backup_path, const char* restore_path);

    /* ===== Utility Functions ===== */

    /* Parse version string (e.g., "1.2.3") */
    bool rogue_config_parse_version_string(const char* version_str, RogueConfigVersion* version);

    /* Format version to string */
    void rogue_config_format_version_string(const RogueConfigVersion* version, char* output,
                                            size_t output_size);

    /* Get human-readable validation result description */
    const char* rogue_config_get_validation_result_description(RogueConfigValidationResult result);

/* Helper macro for registering event types with automatic source tracking */
#define ROGUE_EVENT_REGISTER_SAFE(id, name)                                                        \
    rogue_event_type_register_safe((id), (name), __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif
