#ifndef ROGUE_CFG_MIGRATION_H
#define ROGUE_CFG_MIGRATION_H

#include "../cfg_parser.h"
#include "../json_parser.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration - avoid schema dependency for now
    typedef struct RogueJsonSchema RogueJsonSchema;

    // Migration result status
    typedef enum
    {
        ROGUE_MIGRATION_SUCCESS = 0,
        ROGUE_MIGRATION_FILE_ERROR,
        ROGUE_MIGRATION_PARSE_ERROR,
        ROGUE_MIGRATION_VALIDATION_ERROR,
        ROGUE_MIGRATION_WRITE_ERROR,
        ROGUE_MIGRATION_SCHEMA_ERROR
    } RogueMigrationStatus;

    // Migration statistics
    typedef struct
    {
        int total_files;
        int successful_files;
        int failed_files;
        int total_records;
        int successful_records;
        int failed_records;
        int validation_errors;
        int schema_errors;
    } RogueMigrationStats;

    // Migration configuration
    typedef struct
    {
        const char* source_dir;
        const char* target_dir;
        bool validate_schemas;
        bool create_backup;
        bool overwrite_existing;
        RogueJsonSchema* item_schema;
        RogueJsonSchema* affix_schema;
    } RogueMigrationConfig;

    // Single file migration result
    typedef struct
    {
        RogueMigrationStatus status;
        char source_file[256];
        char target_file[256];
        int records_processed;
        int records_migrated;
        int validation_errors;
        char error_message[1024];
    } RogueMigrationResult;

    // =============================================================================
    // Core Migration API
    // =============================================================================

    /**
     * Initialize migration configuration with default settings
     */
    void rogue_migration_config_init(RogueMigrationConfig* config);

    /**
     * Create required JSON schemas for migration validation
     */
    bool rogue_migration_create_schemas(RogueMigrationConfig* config);

    /**
     * Cleanup migration configuration and schemas
     */
    void rogue_migration_config_cleanup(RogueMigrationConfig* config);

    // =============================================================================
    // Phase 2.3.1: Items & Equipment Migration
    // =============================================================================

    /**
     * Migrate all item configuration files to JSON format
     * Covers test_items.cfg and any other item definition files
     */
    RogueMigrationResult rogue_migrate_items(const RogueMigrationConfig* config);

    /**
     * Migrate single item CFG file to JSON
     */
    RogueMigrationResult rogue_migrate_item_file(const char* source_path, const char* target_path,
                                                 const RogueJsonSchema* schema);

    /**
     * Convert parsed CFG item record to JSON object
     */
    RogueJsonValue* rogue_cfg_item_to_json(const RogueCfgParseResult* cfg_data, int record_index);

    /**
     * Validate migrated item against schema and business rules
     */
    bool rogue_validate_migrated_item(const RogueJsonValue* item, const RogueJsonSchema* schema,
                                      char* error_msg, size_t error_size);

    // =============================================================================
    // Phase 2.3.2: Affixes & Modifiers Migration
    // =============================================================================

    /**
     * Migrate affix configuration file to JSON format
     * Covers affixes.cfg with prefix/suffix definitions
     */
    RogueMigrationResult rogue_migrate_affixes(const RogueMigrationConfig* config);

    /**
     * Convert parsed CFG affix record to JSON object
     */
    RogueJsonValue* rogue_cfg_affix_to_json(const RogueCfgParseResult* cfg_data, int record_index);

    /**
     * Validate migrated affix against schema and business rules
     */
    bool rogue_validate_migrated_affix(const RogueJsonValue* affix, const RogueJsonSchema* schema,
                                       char* error_msg, size_t error_size);

    // =============================================================================
    // Cross-Reference Validation
    // =============================================================================

    /**
     * Validate item ID uniqueness across all migrated files
     */
    bool rogue_validate_item_id_uniqueness(const char* items_dir, char* error_msg,
                                           size_t error_size);

    /**
     * Create cross-reference validation for items and affixes
     */
    bool rogue_validate_item_affix_references(const char* items_dir, const char* affixes_file,
                                              char* error_msg, size_t error_size);

    /**
     * Validate item balance (stat ranges, power budget)
     */
    bool rogue_validate_item_balance(const RogueJsonValue* item, char* error_msg,
                                     size_t error_size);

    /**
     * Validate affix budget (prevent overpowered combinations)
     */
    bool rogue_validate_affix_budget(const RogueJsonValue* affix, char* error_msg,
                                     size_t error_size);

    // =============================================================================
    // Migration Batch Processing
    // =============================================================================

    /**
     * Execute complete Phase 2.3.1 migration (Items & Equipment)
     */
    RogueMigrationStats rogue_migrate_phase_2_3_1(const RogueMigrationConfig* config);

    /**
     * Execute complete Phase 2.3.2 migration (Affixes & Modifiers)
     */
    RogueMigrationStats rogue_migrate_phase_2_3_2(const RogueMigrationConfig* config);

    /**
     * Print migration statistics and results
     */
    void rogue_migration_print_stats(const RogueMigrationStats* stats);

    /**
     * Print migration result details
     */
    void rogue_migration_print_result(const RogueMigrationResult* result);

    // =============================================================================
    // Schema Definitions
    // =============================================================================

    /**
     * Create JSON schema for item validation
     */
    RogueJsonSchema* rogue_create_item_schema(void);

    /**
     * Create JSON schema for affix validation
     */
    RogueJsonSchema* rogue_create_affix_schema(void);

#ifdef __cplusplus
}
#endif

#endif // ROGUE_CFG_MIGRATION_H
