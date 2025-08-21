#include "cfg_migration.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Windows compatibility
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#pragma warning(push)
#pragma warning(disable : 4996) // Disable MSVC secure warnings for strncpy, fopen
#else
#include <sys/stat.h>
#endif

// Logging macros (consistent with existing codebase)
#define ROGUE_LOG_DEBUG(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define ROGUE_LOG_INFO(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define ROGUE_LOG_WARN(fmt, ...) printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#define ROGUE_LOG_ERROR(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)

// =============================================================================
// Helper Functions
// =============================================================================

static bool create_directory_recursive(const char* path)
{
    char tmp[512];
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
            mkdir(tmp, 0755);
            *p = '/'; // Normalize to forward slash
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static bool file_exists(const char* path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

// =============================================================================
// Migration Configuration (Simplified)
// =============================================================================

void rogue_migration_config_init(RogueMigrationConfig* config)
{
    if (!config)
        return;

    memset(config, 0, sizeof(RogueMigrationConfig));
    config->source_dir = "assets";
    config->target_dir = "assets/json";
    config->validate_schemas = false; // Simplified for now
    config->create_backup = true;
    config->overwrite_existing = false;
}

bool rogue_migration_create_schemas(RogueMigrationConfig* config)
{
    if (!config)
        return false;

    // For now, skip schema creation - focus on basic migration
    config->item_schema = NULL;
    config->affix_schema = NULL;
    return true;
}

void rogue_migration_config_cleanup(RogueMigrationConfig* config)
{
    if (!config)
        return;

    // Nothing to cleanup in simplified version
    config->item_schema = NULL;
    config->affix_schema = NULL;
}

// =============================================================================
// Schema Definitions (Stubs for now)
// =============================================================================

RogueJsonSchema* rogue_create_item_schema(void)
{
    // Stub implementation - return NULL for now
    return NULL;
}

RogueJsonSchema* rogue_create_affix_schema(void)
{
    // Stub implementation - return NULL for now
    return NULL;
}

// =============================================================================
// Items Migration (Phase 2.3.1) - Simplified
// =============================================================================

RogueJsonValue* rogue_cfg_item_to_json(const RogueCfgParseResult* cfg_data, int record_index)
{
    if (!cfg_data || !cfg_data->parse_success)
        return NULL;

    // Only handle CSV format for now
    if (cfg_data->detected_format != ROGUE_CFG_FORMAT_CSV)
    {
        ROGUE_LOG_WARN("Unsupported format for item migration");
        return NULL;
    }

    if (record_index >= cfg_data->data.csv.record_count)
        return NULL;

    const RogueCfgRecord* record = &cfg_data->data.csv.records[record_index];
    RogueJsonValue* item = json_create_object();

    if (!item)
        return NULL;

    // Parse CSV format:
    // id,name,category,level_req,stack_max,base_value,dmg_min,dmg_max,armor,sheet,tx,ty,tw,th,rarity
    if (record->count >= 15)
    {
        json_object_set(item, "id", json_create_string(record->values[0]));
        json_object_set(item, "name", json_create_string(record->values[1]));
        json_object_set(item, "category", json_create_integer(atoi(record->values[2])));
        json_object_set(item, "level_req", json_create_integer(atoi(record->values[3])));
        json_object_set(item, "stack_max", json_create_integer(atoi(record->values[4])));
        json_object_set(item, "base_value", json_create_integer(atoi(record->values[5])));
        json_object_set(item, "base_damage_min", json_create_integer(atoi(record->values[6])));
        json_object_set(item, "base_damage_max", json_create_integer(atoi(record->values[7])));
        json_object_set(item, "base_armor", json_create_integer(atoi(record->values[8])));
        json_object_set(item, "sprite_path", json_create_string(record->values[9]));
        json_object_set(item, "sprite_tx", json_create_integer(atoi(record->values[10])));
        json_object_set(item, "sprite_ty", json_create_integer(atoi(record->values[11])));
        json_object_set(item, "sprite_tw", json_create_integer(atoi(record->values[12])));
        json_object_set(item, "sprite_th", json_create_integer(atoi(record->values[13])));
        json_object_set(item, "rarity", json_create_integer(atoi(record->values[14])));
        json_object_set(item, "flags", json_create_integer(0)); // Default value
    }

    return item;
}

bool rogue_validate_migrated_item(const RogueJsonValue* item, const RogueJsonSchema* schema,
                                  char* error_msg, size_t error_size)
{
    // Suppress unreferenced parameter warning
    (void) schema;

    if (!item)
    {
        snprintf(error_msg, error_size, "Invalid item");
        return false;
    }

    // Simplified validation - just check if it's an object
    if (item->type != JSON_OBJECT)
    {
        snprintf(error_msg, error_size, "Item must be a JSON object");
        return false;
    }

    // Additional business rule validation
    RogueJsonValue* category = json_object_get(item, "category");
    RogueJsonValue* damage_min = json_object_get(item, "base_damage_min");
    RogueJsonValue* damage_max = json_object_get(item, "base_damage_max");

    if (category && damage_min && damage_max)
    {
        int cat_val = (int) category->data.integer_value;
        int min_val = (int) damage_min->data.integer_value;
        int max_val = (int) damage_max->data.integer_value;

        // Weapons (category 2) should have damage > 0
        if (cat_val == 2 && (min_val <= 0 || max_val <= 0))
        {
            snprintf(error_msg, error_size, "Weapon items must have positive damage values");
            return false;
        }

        // Damage min should not exceed damage max
        if (min_val > max_val)
        {
            snprintf(error_msg, error_size, "Minimum damage cannot exceed maximum damage");
            return false;
        }
    }

    return true;
}

// Helper function to write JSON value to file with formatting
static void write_json_value_formatted(FILE* f, const RogueJsonValue* value, int indent)
{
    if (!f || !value)
        return;

    switch (value->type)
    {
    case JSON_NULL:
        fprintf(f, "null");
        break;
    case JSON_BOOLEAN:
        fprintf(f, "%s", value->data.boolean_value ? "true" : "false");
        break;
    case JSON_INTEGER:
        fprintf(f, "%lld", (long long) value->data.integer_value);
        break;
    case JSON_NUMBER:
        fprintf(f, "%.6f", value->data.number_value);
        break;
    case JSON_STRING:
        fprintf(f, "\"%s\"", value->data.string_value ? value->data.string_value : "");
        break;
    case JSON_OBJECT:
    {
        fprintf(f, "{\n");
        JsonObject* obj = (JsonObject*) &value->data.object_value;
        for (size_t i = 0; i < obj->count; i++)
        {
            for (int j = 0; j < indent + 2; j++)
                fprintf(f, " ");
            fprintf(f, "\"%s\": ", obj->keys[i]);
            write_json_value_formatted(f, obj->values[i], indent + 2);
            if (i < obj->count - 1)
                fprintf(f, ",");
            fprintf(f, "\n");
        }
        for (int j = 0; j < indent; j++)
            fprintf(f, " ");
        fprintf(f, "}");
        break;
    }
    case JSON_ARRAY:
    {
        fprintf(f, "[\n");
        JsonArray* arr = (JsonArray*) &value->data.array_value;
        for (size_t i = 0; i < arr->count; i++)
        {
            for (int j = 0; j < indent + 2; j++)
                fprintf(f, " ");
            write_json_value_formatted(f, arr->items[i], indent + 2);
            if (i < arr->count - 1)
                fprintf(f, ",");
            fprintf(f, "\n");
        }
        for (int j = 0; j < indent; j++)
            fprintf(f, " ");
        fprintf(f, "]");
        break;
    }
    }
}

RogueMigrationResult rogue_migrate_item_file(const char* source_path, const char* target_path,
                                             const RogueJsonSchema* schema)
{
    RogueMigrationResult result = {0};
    strncpy(result.source_file, source_path, sizeof(result.source_file) - 1);
    strncpy(result.target_file, target_path, sizeof(result.target_file) - 1);

    // Parse source CFG file - returns pointer
    RogueCfgParseResult* cfg_data = rogue_cfg_parse_file(source_path);
    if (!cfg_data || !cfg_data->parse_success)
    {
        result.status = ROGUE_MIGRATION_PARSE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message), "Failed to parse CFG file: %s",
                 source_path);
        if (cfg_data)
            rogue_cfg_free_parse_result(cfg_data);
        return result;
    }

    // Create JSON array for items
    RogueJsonValue* items_array = json_create_array();
    if (!items_array)
    {
        result.status = ROGUE_MIGRATION_PARSE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message), "Failed to create JSON array");
        rogue_cfg_free_parse_result(cfg_data);
        return result;
    }

    // Convert each CFG record to JSON
    result.records_processed = cfg_data->data.csv.record_count;
    for (int i = 0; i < cfg_data->data.csv.record_count; i++)
    {
        RogueJsonValue* item_json = rogue_cfg_item_to_json(cfg_data, i);
        if (item_json)
        {
            char error_msg[512];
            if (schema &&
                !rogue_validate_migrated_item(item_json, schema, error_msg, sizeof(error_msg)))
            {
                ROGUE_LOG_WARN("Item validation failed for record %d: %s", i, error_msg);
                result.validation_errors++;
                json_free(item_json);
                continue;
            }

            json_array_add(items_array, item_json);
            result.records_migrated++;
        }
    }

    // Write JSON to target file
    const char* target_dir = strrchr(target_path, '/');
    if (!target_dir)
        target_dir = strrchr(target_path, '\\');
    if (target_dir)
    {
        char dir_path[512];
        size_t dir_len = target_dir - target_path;
        strncpy(dir_path, target_path, dir_len);
        dir_path[dir_len] = '\0';
        create_directory_recursive(dir_path);
    }

    FILE* output_file = fopen(target_path, "w");
    if (!output_file)
    {
        result.status = ROGUE_MIGRATION_WRITE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message),
                 "Failed to create output file: %s", target_path);
        json_free(items_array);
        rogue_cfg_free_parse_result(cfg_data);
        return result;
    }

    // Write formatted JSON
    write_json_value_formatted(output_file, items_array, 0);
    fprintf(output_file, "\n");

    fclose(output_file);
    result.status = ROGUE_MIGRATION_SUCCESS;

    // Cleanup
    json_free(items_array);
    rogue_cfg_free_parse_result(cfg_data);

    return result;
}

RogueMigrationResult rogue_migrate_items(const RogueMigrationConfig* config)
{
    RogueMigrationResult result = {0};

    // Migrate test_items.cfg to items.json
    char source_path[512];
    char target_path[512];

    snprintf(source_path, sizeof(source_path), "%s/test_items.cfg", config->source_dir);
    snprintf(target_path, sizeof(target_path), "%s/items/items.json", config->target_dir);

    if (!file_exists(source_path))
    {
        result.status = ROGUE_MIGRATION_FILE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message), "Source file not found: %s",
                 source_path);
        return result;
    }

    return rogue_migrate_item_file(source_path, target_path, config->item_schema);
}

// =============================================================================
// Affixes Migration (Phase 2.3.2) - Simplified
// =============================================================================

RogueJsonValue* rogue_cfg_affix_to_json(const RogueCfgParseResult* cfg_data, int record_index)
{
    if (!cfg_data || !cfg_data->parse_success)
        return NULL;

    if (cfg_data->detected_format != ROGUE_CFG_FORMAT_CSV)
    {
        ROGUE_LOG_WARN("Unsupported format for affix migration");
        return NULL;
    }

    if (record_index >= cfg_data->data.csv.record_count)
        return NULL;

    const RogueCfgRecord* record = &cfg_data->data.csv.records[record_index];
    RogueJsonValue* affix = json_create_object();

    if (!affix)
        return NULL;

    // Parse CSV format: type,id,stat,min,max,w_common,w_uncommon,w_rare,w_epic,w_legendary
    if (record->count >= 10)
    {
        json_object_set(affix, "type", json_create_string(record->values[0]));
        json_object_set(affix, "id", json_create_string(record->values[1]));
        json_object_set(affix, "stat", json_create_string(record->values[2]));
        json_object_set(affix, "min_value", json_create_integer(atoi(record->values[3])));
        json_object_set(affix, "max_value", json_create_integer(atoi(record->values[4])));
        json_object_set(affix, "weight_common", json_create_integer(atoi(record->values[5])));
        json_object_set(affix, "weight_uncommon", json_create_integer(atoi(record->values[6])));
        json_object_set(affix, "weight_rare", json_create_integer(atoi(record->values[7])));
        json_object_set(affix, "weight_epic", json_create_integer(atoi(record->values[8])));
        json_object_set(affix, "weight_legendary", json_create_integer(atoi(record->values[9])));
    }

    return affix;
}

bool rogue_validate_migrated_affix(const RogueJsonValue* affix, const RogueJsonSchema* schema,
                                   char* error_msg, size_t error_size)
{
    // Suppress unreferenced parameter warning
    (void) schema;

    if (!affix)
    {
        snprintf(error_msg, error_size, "Invalid affix");
        return false;
    }

    if (affix->type != JSON_OBJECT)
    {
        snprintf(error_msg, error_size, "Affix must be a JSON object");
        return false;
    }

    // Business rule validation
    RogueJsonValue* type_val = json_object_get(affix, "type");
    RogueJsonValue* min_val = json_object_get(affix, "min_value");
    RogueJsonValue* max_val = json_object_get(affix, "max_value");

    if (type_val && type_val->type == JSON_STRING)
    {
        const char* type_str = type_val->data.string_value;
        if (type_str && strcmp(type_str, "PREFIX") != 0 && strcmp(type_str, "SUFFIX") != 0)
        {
            snprintf(error_msg, error_size, "Invalid affix type: %s (must be PREFIX or SUFFIX)",
                     type_str);
            return false;
        }
    }

    if (min_val && max_val)
    {
        int min_int = (int) min_val->data.integer_value;
        int max_int = (int) max_val->data.integer_value;
        if (min_int > max_int)
        {
            snprintf(error_msg, error_size, "Minimum value cannot exceed maximum value");
            return false;
        }
    }

    return true;
}

RogueMigrationResult rogue_migrate_affixes(const RogueMigrationConfig* config)
{
    RogueMigrationResult result = {0};

    char source_path[512];
    char target_path[512];

    snprintf(source_path, sizeof(source_path), "%s/affixes.cfg", config->source_dir);
    snprintf(target_path, sizeof(target_path), "%s/items/affixes.json", config->target_dir);

    strncpy(result.source_file, source_path, sizeof(result.source_file) - 1);
    strncpy(result.target_file, target_path, sizeof(result.target_file) - 1);

    if (!file_exists(source_path))
    {
        result.status = ROGUE_MIGRATION_FILE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message), "Source file not found: %s",
                 source_path);
        return result;
    }

    // Parse source CFG file
    RogueCfgParseResult* cfg_data = rogue_cfg_parse_file(source_path);
    if (!cfg_data || !cfg_data->parse_success)
    {
        result.status = ROGUE_MIGRATION_PARSE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message), "Failed to parse CFG file: %s",
                 source_path);
        if (cfg_data)
            rogue_cfg_free_parse_result(cfg_data);
        return result;
    }

    // Create JSON array for affixes
    RogueJsonValue* affixes_array = json_create_array();
    if (!affixes_array)
    {
        result.status = ROGUE_MIGRATION_PARSE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message), "Failed to create JSON array");
        rogue_cfg_free_parse_result(cfg_data);
        return result;
    }

    // Convert each CFG record to JSON
    result.records_processed = cfg_data->data.csv.record_count;
    for (int i = 0; i < cfg_data->data.csv.record_count; i++)
    {
        RogueJsonValue* affix_json = rogue_cfg_affix_to_json(cfg_data, i);
        if (affix_json)
        {
            char error_msg[512];
            if (config->affix_schema &&
                !rogue_validate_migrated_affix(affix_json, config->affix_schema, error_msg,
                                               sizeof(error_msg)))
            {
                ROGUE_LOG_WARN("Affix validation failed for record %d: %s", i, error_msg);
                result.validation_errors++;
                json_free(affix_json);
                continue;
            }

            json_array_add(affixes_array, affix_json);
            result.records_migrated++;
        }
    }

    // Create target directory
    const char* target_dir = strrchr(target_path, '/');
    if (!target_dir)
        target_dir = strrchr(target_path, '\\');
    if (target_dir)
    {
        char dir_path[512];
        size_t dir_len = target_dir - target_path;
        strncpy(dir_path, target_path, dir_len);
        dir_path[dir_len] = '\0';
        create_directory_recursive(dir_path);
    }

    // Write JSON to target file
    FILE* output_file = fopen(target_path, "w");
    if (!output_file)
    {
        result.status = ROGUE_MIGRATION_WRITE_ERROR;
        snprintf(result.error_message, sizeof(result.error_message),
                 "Failed to create output file: %s", target_path);
        json_free(affixes_array);
        rogue_cfg_free_parse_result(cfg_data);
        return result;
    }

    // Write formatted JSON
    write_json_value_formatted(output_file, affixes_array, 0);
    fprintf(output_file, "\n");

    fclose(output_file);
    result.status = ROGUE_MIGRATION_SUCCESS;

    // Cleanup
    json_free(affixes_array);
    rogue_cfg_free_parse_result(cfg_data);

    return result;
}

// =============================================================================
// Cross-Reference Validation (Stubs)
// =============================================================================

bool rogue_validate_item_id_uniqueness(const char* items_dir, char* error_msg, size_t error_size)
{
    // Implementation stub - would scan all JSON files for duplicate IDs
    (void) error_msg;
    (void) error_size; // Suppress warnings
    ROGUE_LOG_INFO("Item ID uniqueness validation: %s", items_dir);
    return true;
}

bool rogue_validate_item_balance(const RogueJsonValue* item, char* error_msg, size_t error_size)
{
    // Implementation stub - would check stat ranges and power budget
    (void) item;
    (void) error_msg;
    (void) error_size; // Suppress warnings
    return true;
}

bool rogue_validate_affix_budget(const RogueJsonValue* affix, char* error_msg, size_t error_size)
{
    // Implementation stub - would check affix power levels
    (void) affix;
    (void) error_msg;
    (void) error_size; // Suppress warnings
    return true;
}

// =============================================================================
// Batch Processing (Phase 2.3)
// =============================================================================

RogueMigrationStats rogue_migrate_phase_2_3_1(const RogueMigrationConfig* config)
{
    RogueMigrationStats stats = {0};

    ROGUE_LOG_INFO("Starting Phase 2.3.1: Items & Equipment Migration");

    // Migrate items
    RogueMigrationResult item_result = rogue_migrate_items(config);
    stats.total_files++;
    if (item_result.status == ROGUE_MIGRATION_SUCCESS)
    {
        stats.successful_files++;
        stats.total_records += item_result.records_processed;
        stats.successful_records += item_result.records_migrated;
        stats.validation_errors += item_result.validation_errors;
        ROGUE_LOG_INFO("Items migration completed: %d/%d records migrated",
                       item_result.records_migrated, item_result.records_processed);
    }
    else
    {
        stats.failed_files++;
        ROGUE_LOG_ERROR("Items migration failed: %s", item_result.error_message);
    }

    return stats;
}

RogueMigrationStats rogue_migrate_phase_2_3_2(const RogueMigrationConfig* config)
{
    RogueMigrationStats stats = {0};

    ROGUE_LOG_INFO("Starting Phase 2.3.2: Affixes & Modifiers Migration");

    // Migrate affixes
    RogueMigrationResult affix_result = rogue_migrate_affixes(config);
    stats.total_files++;
    if (affix_result.status == ROGUE_MIGRATION_SUCCESS)
    {
        stats.successful_files++;
        stats.total_records += affix_result.records_processed;
        stats.successful_records += affix_result.records_migrated;
        stats.validation_errors += affix_result.validation_errors;
        ROGUE_LOG_INFO("Affixes migration completed: %d/%d records migrated",
                       affix_result.records_migrated, affix_result.records_processed);
    }
    else
    {
        stats.failed_files++;
        ROGUE_LOG_ERROR("Affixes migration failed: %s", affix_result.error_message);
    }

    return stats;
}

// =============================================================================
// Utility Functions
// =============================================================================

void rogue_migration_print_stats(const RogueMigrationStats* stats)
{
    if (!stats)
        return;

    ROGUE_LOG_INFO("=== Migration Statistics ===");
    ROGUE_LOG_INFO("Files: %d total, %d successful, %d failed", stats->total_files,
                   stats->successful_files, stats->failed_files);
    ROGUE_LOG_INFO("Records: %d total, %d migrated", stats->total_records,
                   stats->successful_records);
    ROGUE_LOG_INFO("Validation errors: %d", stats->validation_errors);
    ROGUE_LOG_INFO("Schema errors: %d", stats->schema_errors);
}

void rogue_migration_print_result(const RogueMigrationResult* result)
{
    if (!result)
        return;

    const char* status_str;
    switch (result->status)
    {
    case ROGUE_MIGRATION_SUCCESS:
        status_str = "SUCCESS";
        break;
    case ROGUE_MIGRATION_FILE_ERROR:
        status_str = "FILE_ERROR";
        break;
    case ROGUE_MIGRATION_PARSE_ERROR:
        status_str = "PARSE_ERROR";
        break;
    case ROGUE_MIGRATION_VALIDATION_ERROR:
        status_str = "VALIDATION_ERROR";
        break;
    case ROGUE_MIGRATION_WRITE_ERROR:
        status_str = "WRITE_ERROR";
        break;
    case ROGUE_MIGRATION_SCHEMA_ERROR:
        status_str = "SCHEMA_ERROR";
        break;
    default:
        status_str = "UNKNOWN";
        break;
    }

    ROGUE_LOG_INFO("Migration Result: %s", status_str);
    ROGUE_LOG_INFO("  Source: %s", result->source_file);
    ROGUE_LOG_INFO("  Target: %s", result->target_file);
    ROGUE_LOG_INFO("  Records: %d processed, %d migrated", result->records_processed,
                   result->records_migrated);
    if (result->validation_errors > 0)
    {
        ROGUE_LOG_INFO("  Validation errors: %d", result->validation_errors);
    }
    if (result->error_message[0] != '\0')
    {
        ROGUE_LOG_INFO("  Error: %s", result->error_message);
    }
}

#ifdef _WIN32
#pragma warning(pop)
#endif
