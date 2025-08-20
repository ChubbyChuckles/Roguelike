#ifndef ROGUE_CFG_PARSER_H
#define ROGUE_CFG_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "json_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Constants & Limits ===== */
#define ROGUE_CFG_MAX_LINE_LENGTH 1024
#define ROGUE_CFG_MAX_FIELDS 32
#define ROGUE_CFG_MAX_FILENAME 256
#define ROGUE_CFG_MAX_COMMENT_LENGTH 512
#define ROGUE_CFG_MAX_ANALYSIS_FILES 256

/* ===== CFG File Analysis Types ===== */

typedef enum {
    ROGUE_CFG_DATA_TYPE_UNKNOWN = 0,
    ROGUE_CFG_DATA_TYPE_INTEGER,
    ROGUE_CFG_DATA_TYPE_FLOAT,
    ROGUE_CFG_DATA_TYPE_STRING,
    ROGUE_CFG_DATA_TYPE_ENUM,
    ROGUE_CFG_DATA_TYPE_BOOLEAN,
    ROGUE_CFG_DATA_TYPE_PATH,
    ROGUE_CFG_DATA_TYPE_ID,
    ROGUE_CFG_DATA_TYPE_COUNT
} RogueCfgDataType;

typedef enum {
    ROGUE_CFG_FORMAT_CSV = 0,      /* Comma-separated values */
    ROGUE_CFG_FORMAT_KEY_VALUE,    /* key=value pairs */
    ROGUE_CFG_FORMAT_SECTIONED,    /* [section] headers with entries */
    ROGUE_CFG_FORMAT_TABLE,        /* Table with column headers */
    ROGUE_CFG_FORMAT_HIERARCHICAL, /* Nested structure */
    ROGUE_CFG_FORMAT_LIST,         /* Simple list entries */
    ROGUE_CFG_FORMAT_COUNT
} RogueCfgFormat;

typedef enum {
    ROGUE_CFG_CATEGORY_ITEMS = 0,
    ROGUE_CFG_CATEGORY_AFFIXES,
    ROGUE_CFG_CATEGORY_LOOT_TABLES,
    ROGUE_CFG_CATEGORY_TILES,
    ROGUE_CFG_CATEGORY_SOUNDS,
    ROGUE_CFG_CATEGORY_DIALOGUE,
    ROGUE_CFG_CATEGORY_SKILLS,
    ROGUE_CFG_CATEGORY_ENEMIES,
    ROGUE_CFG_CATEGORY_BIOMES,
    ROGUE_CFG_CATEGORY_MATERIALS,
    ROGUE_CFG_CATEGORY_RESOURCES,
    ROGUE_CFG_CATEGORY_UI,
    ROGUE_CFG_CATEGORY_ENCOUNTERS,
    ROGUE_CFG_CATEGORY_PLAYER,
    ROGUE_CFG_CATEGORY_MISC,
    ROGUE_CFG_CATEGORY_COUNT
} RogueCfgCategory;

typedef struct {
    char name[64];
    RogueCfgDataType type;
    bool is_nullable;
    bool is_id_field;
    int min_value;
    int max_value;
    char enum_values[16][32];  /* For enum types */
    int enum_count;
} RogueCfgFieldInfo;

typedef struct {
    char filename[ROGUE_CFG_MAX_FILENAME];
    RogueCfgCategory category;
    RogueCfgFormat format;
    
    /* Format analysis */
    bool has_header_comment;
    char header_comment[ROGUE_CFG_MAX_COMMENT_LENGTH];
    int field_count;
    RogueCfgFieldInfo fields[ROGUE_CFG_MAX_FIELDS];
    
    /* Content analysis */
    int total_lines;
    int data_lines;
    int comment_lines;
    int empty_lines;
    bool has_inconsistent_format;
    
    /* Data validation */
    bool has_duplicate_ids;
    bool has_missing_required_fields;
    int validation_error_count;
    char validation_errors[16][256];
} RogueCfgFileAnalysis;

typedef struct {
    int total_files;
    RogueCfgFileAnalysis files[ROGUE_CFG_MAX_ANALYSIS_FILES];
    
    /* Summary statistics */
    int files_by_category[ROGUE_CFG_CATEGORY_COUNT];
    int files_by_format[ROGUE_CFG_FORMAT_COUNT];
    int total_data_lines;
    int total_validation_errors;
    
    /* Migration recommendations */
    int high_priority_files; /* Complex files requiring immediate migration */
    int medium_priority_files; /* Standard files with straightforward migration */
    int low_priority_files; /* Simple files, can be migrated later */
} RogueCfgAnalysisReport;

/* ===== CFG Parser Types ===== */

typedef struct {
    int line_number;
    int column_number;
    char message[256];
} RogueCfgParseError;

typedef struct {
    char key[128];
    char value[512];
} RogueCfgKeyValuePair;

typedef struct {
    char values[ROGUE_CFG_MAX_FIELDS][256];
    int count;
} RogueCfgRecord;

typedef struct {
    char filename[ROGUE_CFG_MAX_FILENAME];
    RogueCfgFormat detected_format;
    
    /* Parsed data - union based on format */
    union {
        struct {
            RogueCfgRecord* records;
            int record_count;
            char headers[ROGUE_CFG_MAX_FIELDS][64];
            int header_count;
        } csv;
        
        struct {
            RogueCfgKeyValuePair* pairs;
            int pair_count;
        } key_value;
    } data;
    
    /* Parse status */
    bool parse_success;
    int error_count;
    RogueCfgParseError errors[64];
    
    /* Recovery information */
    int recovered_lines; /* Lines that were partially recovered despite errors */
    int skipped_lines;   /* Lines that were completely skipped due to errors */
} RogueCfgParseResult;

/* ===== Migration Types ===== */

typedef struct {
    char source_file[ROGUE_CFG_MAX_FILENAME];
    char target_file[ROGUE_CFG_MAX_FILENAME];
    RogueCfgCategory category;
    
    /* Conversion status */
    bool migration_success;
    int records_converted;
    int records_failed;
    
    /* Data validation results */
    int validation_warnings;
    int validation_errors;
    char conversion_notes[1024];
    
    /* Schema information */
    char schema_name[128];
    int schema_version;
} RogueCfgMigrationResult;

/* ===== Function Declarations ===== */

/* File Analysis Functions */
RogueCfgAnalysisReport* rogue_cfg_analyze_directory(const char* directory_path);
RogueCfgFileAnalysis* rogue_cfg_analyze_file(const char* filename);
RogueCfgCategory rogue_cfg_classify_file(const char* filename);
RogueCfgFormat rogue_cfg_detect_format(const char* filename);
bool rogue_cfg_validate_file(const RogueCfgFileAnalysis* analysis);

/* CFG Parser Functions */
RogueCfgParseResult* rogue_cfg_parse_file(const char* filename);
bool rogue_cfg_parse_csv_line(const char* line, RogueCfgRecord* record);
bool rogue_cfg_parse_key_value_line(const char* line, RogueCfgKeyValuePair* pair);
bool rogue_cfg_is_comment_line(const char* line);
bool rogue_cfg_is_empty_line(const char* line);

/* Migration Functions */
RogueCfgMigrationResult* rogue_cfg_migrate_to_json(const char* cfg_filename, const char* json_filename);
RogueJsonValue* rogue_cfg_convert_record_to_json(const RogueCfgRecord* record, const RogueCfgFieldInfo* fields, int field_count);
bool rogue_cfg_create_target_schema(RogueCfgCategory category, const char* schema_filename);
bool rogue_cfg_validate_converted_json(const char* json_filename, const char* schema_filename);

/* Batch Migration Functions */
bool rogue_cfg_migrate_category_batch(RogueCfgCategory category, const char* source_dir, const char* target_dir);
bool rogue_cfg_create_migration_report(const RogueCfgMigrationResult* results, int result_count, const char* report_filename);

/* Utility Functions */
const char* rogue_cfg_data_type_to_string(RogueCfgDataType type);
const char* rogue_cfg_format_to_string(RogueCfgFormat format);
const char* rogue_cfg_category_to_string(RogueCfgCategory category);
void rogue_cfg_free_analysis_report(RogueCfgAnalysisReport* report);
void rogue_cfg_free_parse_result(RogueCfgParseResult* result);
void rogue_cfg_free_migration_result(RogueCfgMigrationResult* result);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CFG_PARSER_H */
