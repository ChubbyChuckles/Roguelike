/**
 * @file cfg_parser.c
 * @brief CFG file parser implementation with Windows compatibility support.
 *        Provides comprehensive parsing, analysis, and migration utilities for configuration files.
 */

/* Windows compatibility */
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "cfg_parser.h"
#include "log.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* Windows compatibility */
#ifdef _WIN32
#define strdup _strdup
#include <direct.h>
#include <io.h>
#define stat _stat
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define access _access
#else
#include <dirent.h>
#include <unistd.h>
#endif

/* ===== Static Helper Functions ===== */

/**
 * @brief Detects the data type of a configuration value based on its content.
 *
 * @param value The string value to analyze.
 * @return The detected RogueCfgDataType.
 */
static RogueCfgDataType detect_data_type(const char* value)
{
    if (!value || *value == '\0')
    {
        return ROGUE_CFG_DATA_TYPE_STRING;
    }

    /* Check for boolean values */
    if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0 || strcmp(value, "1") == 0 ||
        strcmp(value, "0") == 0)
    {
        return ROGUE_CFG_DATA_TYPE_BOOLEAN;
    }

    /* Check for file paths */
    if (strstr(value, "/") || strstr(value, "\\") || strstr(value, ".png") ||
        strstr(value, ".jpg") || strstr(value, ".wav") || strstr(value, ".cfg"))
    {
        return ROGUE_CFG_DATA_TYPE_PATH;
    }

    /* Check for integer */
    char* endptr;
    strtol(value, &endptr, 10);
    if (*endptr == '\0' && endptr != value)
    {
        return ROGUE_CFG_DATA_TYPE_INTEGER;
    }

    /* Check for float */
    strtod(value, &endptr);
    if (*endptr == '\0' && endptr != value)
    {
        return ROGUE_CFG_DATA_TYPE_FLOAT;
    }

    /* Check if it looks like an ID (contains underscore, lowercase) */
    bool has_underscore = strchr(value, '_') != NULL;
    bool is_lowercase = true;
    for (const char* p = value; *p && is_lowercase; p++)
    {
        if (isalpha(*p) && !islower(*p))
        {
            is_lowercase = false;
        }
    }

    if (has_underscore && is_lowercase)
    {
        return ROGUE_CFG_DATA_TYPE_ID;
    }

    return ROGUE_CFG_DATA_TYPE_STRING;
}

/**
 * @brief Trims leading and trailing whitespace from a string in-place.
 *
 * @param str The string to trim (modified in-place).
 */
static void trim_whitespace(char* str)
{
    if (!str)
        return;

    /* Trim leading whitespace */
    char* start = str;
    while (*start && isspace((unsigned char) *start))
    {
        start++;
    }

    /* Move trimmed string to beginning */
    if (start != str)
    {
        memmove(str, start, strlen(start) + 1);
    }

    /* Trim trailing whitespace */
    char* end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end))
    {
        *end-- = '\0';
    }
}

/**
 * @brief Checks if a filename has a .cfg extension.
 *
 * @param filename The filename to check.
 * @return true if the file has a .cfg extension, false otherwise.
 */
static bool is_cfg_file(const char* filename)
{
    const char* ext = strrchr(filename, '.');
    return ext && strcmp(ext, ".cfg") == 0;
}

/* ===== File Analysis Implementation ===== */

/**
 * @brief Classifies a CFG file into a category based on its filename patterns.
 *
 * @param filename The name of the CFG file to classify.
 * @return The detected RogueCfgCategory.
 */
RogueCfgCategory rogue_cfg_classify_file(const char* filename)
{
    if (!filename)
        return ROGUE_CFG_CATEGORY_MISC;

    /* Convert to lowercase for pattern matching */
    char lower_filename[ROGUE_CFG_MAX_FILENAME];
    strncpy(lower_filename, filename, sizeof(lower_filename) - 1);
    lower_filename[sizeof(lower_filename) - 1] = '\0';

    for (char* p = lower_filename; *p; p++)
    {
        *p = (char) tolower((unsigned char) *p);
    }

    /* Category classification based on filename patterns */
    if (strstr(lower_filename, "item") || strstr(lower_filename, "equipment") ||
        strstr(lower_filename, "weapon") || strstr(lower_filename, "armor"))
    {
        return ROGUE_CFG_CATEGORY_ITEMS;
    }

    if (strstr(lower_filename, "affix") || strstr(lower_filename, "modifier"))
    {
        return ROGUE_CFG_CATEGORY_AFFIXES;
    }

    if (strstr(lower_filename, "loot") || strstr(lower_filename, "table"))
    {
        return ROGUE_CFG_CATEGORY_LOOT_TABLES;
    }

    if (strstr(lower_filename, "tile") || strstr(lower_filename, "tileset"))
    {
        return ROGUE_CFG_CATEGORY_TILES;
    }

    if (strstr(lower_filename, "sound") || strstr(lower_filename, "audio"))
    {
        return ROGUE_CFG_CATEGORY_SOUNDS;
    }

    if (strstr(lower_filename, "dialogue") || strstr(lower_filename, "avatar"))
    {
        return ROGUE_CFG_CATEGORY_DIALOGUE;
    }

    if (strstr(lower_filename, "skill") || strstr(lower_filename, "abilities") ||
        strstr(lower_filename, "ability"))
    {
        return ROGUE_CFG_CATEGORY_SKILLS;
    }

    if (strstr(lower_filename, "enemy") || strstr(lower_filename, "mob") ||
        strstr(lower_filename, "encounter"))
    {
        return ROGUE_CFG_CATEGORY_ENEMIES;
    }

    if (strstr(lower_filename, "biome") || strstr(lower_filename, "environment"))
    {
        return ROGUE_CFG_CATEGORY_BIOMES;
    }

    if (strstr(lower_filename, "material") || strstr(lower_filename, "resource"))
    {
        return ROGUE_CFG_CATEGORY_MATERIALS;
    }

    if (strstr(lower_filename, "ui") || strstr(lower_filename, "hud") ||
        strstr(lower_filename, "theme"))
    {
        return ROGUE_CFG_CATEGORY_UI;
    }

    if (strstr(lower_filename, "player") || strstr(lower_filename, "stats"))
    {
        return ROGUE_CFG_CATEGORY_PLAYER;
    }

    return ROGUE_CFG_CATEGORY_MISC;
}

/**
 * @brief Detects the format of a CFG file by analyzing its content.
 *
 * @param filename The path to the CFG file to analyze.
 * @return The detected RogueCfgFormat.
 */
RogueCfgFormat rogue_cfg_detect_format(const char* filename)
{
    if (!filename)
        return ROGUE_CFG_FORMAT_CSV;

    FILE* file = fopen(filename, "r");
    if (!file)
    {
        ROGUE_LOG_ERROR("Cannot open file for format detection: %s", filename);
        return ROGUE_CFG_FORMAT_CSV;
    }

    char line[ROGUE_CFG_MAX_LINE_LENGTH];
    RogueCfgFormat detected_format = ROGUE_CFG_FORMAT_CSV;
    bool found_data = false;

    while (fgets(line, sizeof(line), file))
    {
        trim_whitespace(line);

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }

        found_data = true;

        /* Check for key=value format */
        if (strchr(line, '=') && !strchr(line, ','))
        {
            detected_format = ROGUE_CFG_FORMAT_KEY_VALUE;
            break;
        }

        /* Check for section headers [section] */
        if (line[0] == '[' && line[strlen(line) - 1] == ']')
        {
            detected_format = ROGUE_CFG_FORMAT_SECTIONED;
            break;
        }

        /* Check for comma-separated values */
        if (strchr(line, ','))
        {
            detected_format = ROGUE_CFG_FORMAT_CSV;
            break;
        }

        /* If no specific pattern, assume list format */
        detected_format = ROGUE_CFG_FORMAT_LIST;
        break;
    }

    fclose(file);

    if (!found_data)
    {
        ROGUE_LOG_WARN("No data found in file: %s", filename);
    }

    return detected_format;
}

/**
 * @brief Performs comprehensive analysis of a CFG file including format detection,
 *        category classification, and structural analysis.
 *
 * @param filename The path to the CFG file to analyze.
 * @return A pointer to RogueCfgFileAnalysis structure, or NULL on error.
 *         The caller is responsible for freeing the returned structure.
 */
RogueCfgFileAnalysis* rogue_cfg_analyze_file(const char* filename)
{
    if (!filename || !is_cfg_file(filename))
    {
        ROGUE_LOG_ERROR("Invalid CFG filename: %s", filename);
        return NULL;
    }

    RogueCfgFileAnalysis* analysis = calloc(1, sizeof(RogueCfgFileAnalysis));
    if (!analysis)
    {
        ROGUE_LOG_ERROR("Failed to allocate memory for file analysis");
        return NULL;
    }

    /* Initialize analysis structure */
    strncpy(analysis->filename, filename, sizeof(analysis->filename) - 1);
    analysis->category = rogue_cfg_classify_file(filename);
    analysis->format = rogue_cfg_detect_format(filename);

    FILE* file = fopen(filename, "r");
    if (!file)
    {
        ROGUE_LOG_ERROR("Cannot open file for analysis: %s", filename);
        snprintf(analysis->validation_errors[analysis->validation_error_count++],
                 sizeof(analysis->validation_errors[0]), "Cannot open file: %s", filename);
        return analysis;
    }

    char line[ROGUE_CFG_MAX_LINE_LENGTH];
    bool in_header_comment = true;
    char header_buffer[ROGUE_CFG_MAX_COMMENT_LENGTH] = {0};

    while (fgets(line, sizeof(line), file))
    {
        analysis->total_lines++;
        trim_whitespace(line);

        if (line[0] == '\0')
        {
            analysis->empty_lines++;
            continue;
        }

        if (line[0] == '#')
        {
            analysis->comment_lines++;

            /* Collect header comment */
            if (in_header_comment)
            {
                if (strlen(header_buffer) > 0)
                {
                    strncat(header_buffer, "\n", sizeof(header_buffer) - strlen(header_buffer) - 1);
                }
                strncat(header_buffer, line + 1, sizeof(header_buffer) - strlen(header_buffer) - 1);
                analysis->has_header_comment = true;
            }
            continue;
        }

        /* First data line ends header comment collection */
        if (in_header_comment)
        {
            in_header_comment = false;
            strncpy(analysis->header_comment, header_buffer, sizeof(analysis->header_comment) - 1);

            /* If we couldn't classify by filename, try header-based heuristics */
            if (analysis->category == ROGUE_CFG_CATEGORY_MISC)
            {
                if (strstr(header_buffer, "type,id,stat") || strstr(header_buffer, "type,id") ||
                    strstr(header_buffer, "stat,min,max"))
                {
                    analysis->category = ROGUE_CFG_CATEGORY_AFFIXES;
                }
                else if (strstr(header_buffer, "id,name,category") ||
                         strstr(header_buffer, "dmg_min,dmg_max"))
                {
                    analysis->category = ROGUE_CFG_CATEGORY_ITEMS;
                }
            }
        }

        analysis->data_lines++;

        /* Analyze data structure based on format */
        if (analysis->format == ROGUE_CFG_FORMAT_CSV && analysis->field_count == 0)
        {
            /* Parse field structure from first data line */
            char* line_copy = strdup(line);
            char* token = strtok(line_copy, ",");
            /* Content-based category inference if still unknown */
            if (analysis->category == ROGUE_CFG_CATEGORY_MISC && token)
            {
                /* Normalize first token */
                char first_tok[64] = {0};
                strncpy(first_tok, token, sizeof(first_tok) - 1);
                for (char* p = first_tok; *p; ++p)
                    *p = (char) tolower((unsigned char) *p);
                if (strcmp(first_tok, "prefix") == 0 || strcmp(first_tok, "suffix") == 0)
                {
                    analysis->category = ROGUE_CFG_CATEGORY_AFFIXES;
                }
            }

            while (token && analysis->field_count < ROGUE_CFG_MAX_FIELDS)
            {
                trim_whitespace(token);

                RogueCfgFieldInfo* field = &analysis->fields[analysis->field_count];
                snprintf(field->name, sizeof(field->name), "field_%d", analysis->field_count);
                field->type = detect_data_type(token);

                analysis->field_count++;
                token = strtok(NULL, ",");
            }

            free(line_copy);
        }
    }

    fclose(file);

    ROGUE_LOG_INFO("Analyzed CFG file: %s (%s format, %d data lines)", filename,
                   rogue_cfg_format_to_string(analysis->format), analysis->data_lines);

    return analysis;
}

/* ===== CFG Parser Implementation ===== */

/**
 * @brief Checks if a line is a comment line (starts with #).
 *
 * @param line The line to check.
 * @return true if the line is a comment, false otherwise.
 */
bool rogue_cfg_is_comment_line(const char* line)
{
    if (!line)
        return false;
    char* trimmed = strdup(line);
    trim_whitespace(trimmed);
    bool is_comment = (trimmed[0] == '#');
    free(trimmed);
    return is_comment;
}

/**
 * @brief Checks if a line is empty or contains only whitespace.
 *
 * @param line The line to check.
 * @return true if the line is empty, false otherwise.
 */
bool rogue_cfg_is_empty_line(const char* line)
{
    if (!line)
        return true;
    char* trimmed = strdup(line);
    trim_whitespace(trimmed);
    bool is_empty = (trimmed[0] == '\0');
    free(trimmed);
    return is_empty;
}

/**
 * @brief Parses a CSV line into a RogueCfgRecord structure.
 *
 * @param line The CSV line to parse.
 * @param record Pointer to the record structure to fill.
 * @return true if parsing succeeded, false otherwise.
 */
bool rogue_cfg_parse_csv_line(const char* line, RogueCfgRecord* record)
{
    if (!line || !record)
        return false;

    record->count = 0;
    char* line_copy = strdup(line);
    char* token = strtok(line_copy, ",");

    while (token && record->count < ROGUE_CFG_MAX_FIELDS)
    {
        trim_whitespace(token);
        strncpy(record->values[record->count], token, sizeof(record->values[record->count]) - 1);
        record->values[record->count][sizeof(record->values[record->count]) - 1] = '\0';
        record->count++;
        token = strtok(NULL, ",");
    }

    free(line_copy);
    return record->count > 0;
}

/**
 * @brief Parses a key-value line into a RogueCfgKeyValuePair structure.
 *
 * @param line The key-value line to parse (format: key=value).
 * @param pair Pointer to the pair structure to fill.
 * @return true if parsing succeeded, false otherwise.
 */
bool rogue_cfg_parse_key_value_line(const char* line, RogueCfgKeyValuePair* pair)
{
    if (!line || !pair)
        return false;

    char* equals = strchr(line, '=');
    if (!equals)
        return false;

    /* Extract key */
    size_t key_len = equals - line;
    if (key_len >= sizeof(pair->key))
        key_len = sizeof(pair->key) - 1;
    strncpy(pair->key, line, key_len);
    pair->key[key_len] = '\0';
    trim_whitespace(pair->key);

    /* Extract value */
    strncpy(pair->value, equals + 1, sizeof(pair->value) - 1);
    pair->value[sizeof(pair->value) - 1] = '\0';
    trim_whitespace(pair->value);

    return strlen(pair->key) > 0;
}

/**
 * @brief Parses an entire CFG file and returns structured data.
 *
 * @param filename The path to the CFG file to parse.
 * @return A pointer to RogueCfgParseResult structure, or NULL on error.
 *         The caller is responsible for freeing the returned structure.
 */
RogueCfgParseResult* rogue_cfg_parse_file(const char* filename)
{
    if (!filename)
        return NULL;

    RogueCfgParseResult* result = calloc(1, sizeof(RogueCfgParseResult));
    if (!result)
        return NULL;

    strncpy(result->filename, filename, sizeof(result->filename) - 1);
    result->detected_format = rogue_cfg_detect_format(filename);

    FILE* file = fopen(filename, "r");
    if (!file)
    {
        ROGUE_LOG_ERROR("Cannot open file for parsing: %s", filename);
        result->parse_success = false;
        return result;
    }

    char line[ROGUE_CFG_MAX_LINE_LENGTH];
    int line_number = 0;

    /* Allocate initial data structures based on format */
    if (result->detected_format == ROGUE_CFG_FORMAT_CSV)
    {
        result->data.csv.records = malloc(sizeof(RogueCfgRecord) * 1000); /* Initial capacity */
        result->data.csv.record_count = 0;
    }
    else if (result->detected_format == ROGUE_CFG_FORMAT_KEY_VALUE)
    {
        result->data.key_value.pairs = malloc(sizeof(RogueCfgKeyValuePair) * 1000);
        result->data.key_value.pair_count = 0;
    }

    while (fgets(line, sizeof(line), file))
    {
        line_number++;
        trim_whitespace(line);

        /* Skip comments and empty lines */
        if (rogue_cfg_is_comment_line(line) || rogue_cfg_is_empty_line(line))
        {
            continue;
        }

        /* Parse based on detected format */
        if (result->detected_format == ROGUE_CFG_FORMAT_CSV)
        {
            RogueCfgRecord record = {0};
            if (rogue_cfg_parse_csv_line(line, &record))
            {
                result->data.csv.records[result->data.csv.record_count++] = record;
            }
            else
            {
                result->skipped_lines++;
                ROGUE_LOG_WARN("Failed to parse CSV line %d: %s", line_number, line);
            }
        }
        else if (result->detected_format == ROGUE_CFG_FORMAT_KEY_VALUE)
        {
            RogueCfgKeyValuePair pair = {0};
            if (rogue_cfg_parse_key_value_line(line, &pair))
            {
                result->data.key_value.pairs[result->data.key_value.pair_count++] = pair;
            }
            else
            {
                result->skipped_lines++;
                ROGUE_LOG_WARN("Failed to parse key-value line %d: %s", line_number, line);
            }
        }
    }

    fclose(file);
    result->parse_success = true;

    ROGUE_LOG_INFO("Parsed CFG file: %s (%s format, %s)", filename,
                   rogue_cfg_format_to_string(result->detected_format),
                   result->parse_success ? "success" : "failed");

    return result;
}

/* ===== Utility Functions ===== */

/**
 * @brief Converts a RogueCfgDataType enum value to its string representation.
 *
 * @param type The data type to convert.
 * @return String representation of the data type.
 */
const char* rogue_cfg_data_type_to_string(RogueCfgDataType type)
{
    switch (type)
    {
    case ROGUE_CFG_DATA_TYPE_INTEGER:
        return "integer";
    case ROGUE_CFG_DATA_TYPE_FLOAT:
        return "float";
    case ROGUE_CFG_DATA_TYPE_STRING:
        return "string";
    case ROGUE_CFG_DATA_TYPE_ENUM:
        return "enum";
    case ROGUE_CFG_DATA_TYPE_BOOLEAN:
        return "boolean";
    case ROGUE_CFG_DATA_TYPE_PATH:
        return "path";
    case ROGUE_CFG_DATA_TYPE_ID:
        return "id";
    default:
        return "unknown";
    }
}

/**
 * @brief Converts a RogueCfgFormat enum value to its string representation.
 *
 * @param format The format to convert.
 * @return String representation of the format.
 */
const char* rogue_cfg_format_to_string(RogueCfgFormat format)
{
    switch (format)
    {
    case ROGUE_CFG_FORMAT_CSV:
        return "CSV";
    case ROGUE_CFG_FORMAT_KEY_VALUE:
        return "Key-Value";
    case ROGUE_CFG_FORMAT_SECTIONED:
        return "Sectioned";
    case ROGUE_CFG_FORMAT_TABLE:
        return "Table";
    case ROGUE_CFG_FORMAT_HIERARCHICAL:
        return "Hierarchical";
    case ROGUE_CFG_FORMAT_LIST:
        return "List";
    default:
        return "Unknown";
    }
}

/**
 * @brief Converts a RogueCfgCategory enum value to its string representation.
 *
 * @param category The category to convert.
 * @return String representation of the category.
 */
const char* rogue_cfg_category_to_string(RogueCfgCategory category)
{
    switch (category)
    {
    case ROGUE_CFG_CATEGORY_ITEMS:
        return "Items";
    case ROGUE_CFG_CATEGORY_AFFIXES:
        return "Affixes";
    case ROGUE_CFG_CATEGORY_LOOT_TABLES:
        return "Loot Tables";
    case ROGUE_CFG_CATEGORY_TILES:
        return "Tiles";
    case ROGUE_CFG_CATEGORY_SOUNDS:
        return "Sounds";
    case ROGUE_CFG_CATEGORY_DIALOGUE:
        return "Dialogue";
    case ROGUE_CFG_CATEGORY_SKILLS:
        return "Skills";
    case ROGUE_CFG_CATEGORY_ENEMIES:
        return "Enemies";
    case ROGUE_CFG_CATEGORY_BIOMES:
        return "Biomes";
    case ROGUE_CFG_CATEGORY_MATERIALS:
        return "Materials";
    case ROGUE_CFG_CATEGORY_RESOURCES:
        return "Resources";
    case ROGUE_CFG_CATEGORY_UI:
        return "UI";
    case ROGUE_CFG_CATEGORY_ENCOUNTERS:
        return "Encounters";
    case ROGUE_CFG_CATEGORY_PLAYER:
        return "Player";
    case ROGUE_CFG_CATEGORY_MISC:
        return "Miscellaneous";
    default:
        return "Unknown";
    }
}

/**
 * @brief Frees memory allocated for a RogueCfgAnalysisReport structure.
 *
 * @param report Pointer to the report structure to free.
 */
void rogue_cfg_free_analysis_report(RogueCfgAnalysisReport* report)
{
    if (report)
    {
        free(report);
    }
}

/**
 * @brief Frees memory allocated for a RogueCfgParseResult structure and its contents.
 *
 * @param result Pointer to the parse result structure to free.
 */
void rogue_cfg_free_parse_result(RogueCfgParseResult* result)
{
    if (!result)
        return;

    if (result->detected_format == ROGUE_CFG_FORMAT_CSV && result->data.csv.records)
    {
        free(result->data.csv.records);
    }
    else if (result->detected_format == ROGUE_CFG_FORMAT_KEY_VALUE && result->data.key_value.pairs)
    {
        free(result->data.key_value.pairs);
    }

    free(result);
}

/**
 * @brief Frees memory allocated for a RogueCfgMigrationResult structure.
 *
 * @param result Pointer to the migration result structure to free.
 */
void rogue_cfg_free_migration_result(RogueCfgMigrationResult* result)
{
    if (result)
    {
        free(result);
    }
}

/* ===== Stub Functions (Phase 2.2+ Implementation) ===== */

/**
 * @brief Analyzes all CFG files in a directory (stub implementation for Phase 2.2+).
 *
 * @param directory_path The path to the directory to analyze.
 * @return NULL (not yet implemented).
 */
RogueCfgAnalysisReport* rogue_cfg_analyze_directory(const char* directory_path)
{
    ROGUE_LOG_WARN("rogue_cfg_analyze_directory not yet implemented");
    (void) directory_path; /* Suppress unused parameter warning */
    return NULL;
}

/**
 * @brief Validates a CFG file against its expected schema (stub implementation for Phase 2.2+).
 *
 * @param analysis Pointer to the file analysis structure.
 * @return false (not yet implemented).
 */
bool rogue_cfg_validate_file(const RogueCfgFileAnalysis* analysis)
{
    ROGUE_LOG_WARN("rogue_cfg_validate_file not yet implemented");
    (void) analysis; /* Suppress unused parameter warning */
    return false;
}

/**
 * @brief Migrates a CFG file to JSON format (stub implementation for Phase 2.2+).
 *
 * @param cfg_filename The source CFG file path.
 * @param json_filename The target JSON file path.
 * @return NULL (not yet implemented).
 */
RogueCfgMigrationResult* rogue_cfg_migrate_to_json(const char* cfg_filename,
                                                   const char* json_filename)
{
    ROGUE_LOG_WARN("rogue_cfg_migrate_to_json not yet implemented");
    (void) cfg_filename;
    (void) json_filename;
    return NULL;
}

/**
 * @brief Converts a CFG record to JSON format (stub implementation for Phase 2.2+).
 *
 * @param record Pointer to the CFG record to convert.
 * @param fields Array of field information.
 * @param field_count Number of fields in the array.
 * @return NULL (not yet implemented).
 */
RogueJsonValue* rogue_cfg_convert_record_to_json(const RogueCfgRecord* record,
                                                 const RogueCfgFieldInfo* fields, int field_count)
{
    ROGUE_LOG_WARN("rogue_cfg_convert_record_to_json not yet implemented");
    (void) record;
    (void) fields;
    (void) field_count;
    return NULL;
}

/**
 * @brief Creates a JSON schema for a specific CFG category (stub implementation for Phase 2.2+).
 *
 * @param category The CFG category for which to create the schema.
 * @param schema_filename The filename for the generated schema.
 * @return false (not yet implemented).
 */
bool rogue_cfg_create_target_schema(RogueCfgCategory category, const char* schema_filename)
{
    ROGUE_LOG_WARN("rogue_cfg_create_target_schema not yet implemented");
    (void) category;
    (void) schema_filename;
    return false;
}

/**
 * @brief Validates converted JSON against a schema (stub implementation for Phase 2.2+).
 *
 * @param json_filename The JSON file to validate.
 * @param schema_filename The schema file to validate against.
 * @return false (not yet implemented).
 */
bool rogue_cfg_validate_converted_json(const char* json_filename, const char* schema_filename)
{
    ROGUE_LOG_WARN("rogue_cfg_validate_converted_json not yet implemented");
    (void) json_filename;
    (void) schema_filename;
    return false;
}

/**
 * @brief Migrates all CFG files of a specific category in batch (stub implementation for
 * Phase 2.2+).
 *
 * @param category The CFG category to migrate.
 * @param source_dir The source directory containing CFG files.
 * @param target_dir The target directory for JSON files.
 * @return false (not yet implemented).
 */
bool rogue_cfg_migrate_category_batch(RogueCfgCategory category, const char* source_dir,
                                      const char* target_dir)
{
    ROGUE_LOG_WARN("rogue_cfg_migrate_category_batch not yet implemented");
    (void) category;
    (void) source_dir;
    (void) target_dir;
    return false;
}

/**
 * @brief Creates a migration report from results (stub implementation for Phase 2.2+).
 *
 * @param results Array of migration results.
 * @param result_count Number of results in the array.
 * @param report_filename The filename for the generated report.
 * @return false (not yet implemented).
 */
bool rogue_cfg_create_migration_report(const RogueCfgMigrationResult* results, int result_count,
                                       const char* report_filename)
{
    ROGUE_LOG_WARN("rogue_cfg_create_migration_report not yet implemented");
    (void) results;
    (void) result_count;
    (void) report_filename;
    return false;
}
