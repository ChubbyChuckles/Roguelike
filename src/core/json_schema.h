#ifndef ROGUE_JSON_SCHEMA_H
#define ROGUE_JSON_SCHEMA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "json_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Constants & Limits ===== */
#define ROGUE_SCHEMA_MAX_NAME_LENGTH 64
#define ROGUE_SCHEMA_MAX_DESCRIPTION_LENGTH 256
#define ROGUE_SCHEMA_MAX_PATH_LENGTH 512
#define ROGUE_SCHEMA_MAX_FIELDS 128
#define ROGUE_SCHEMA_MAX_NESTED_DEPTH 8
#define ROGUE_SCHEMA_MAX_VALIDATION_ERRORS 64
#define ROGUE_SCHEMA_MAX_DEPENDENCIES 32

/* ===== Core Types ===== */
typedef enum {
    ROGUE_SCHEMA_TYPE_NULL = 0,
    ROGUE_SCHEMA_TYPE_BOOLEAN,
    ROGUE_SCHEMA_TYPE_INTEGER,
    ROGUE_SCHEMA_TYPE_NUMBER,
    ROGUE_SCHEMA_TYPE_STRING,
    ROGUE_SCHEMA_TYPE_ARRAY,
    ROGUE_SCHEMA_TYPE_OBJECT,
    ROGUE_SCHEMA_TYPE_ENUM,
    ROGUE_SCHEMA_TYPE_REFERENCE,  /* Reference to another schema */
    ROGUE_SCHEMA_TYPE_COUNT
} RogueSchemaType;

typedef enum {
    ROGUE_SCHEMA_VERSION_1_0 = 100,
    ROGUE_SCHEMA_VERSION_1_1 = 101,
    ROGUE_SCHEMA_VERSION_CURRENT = ROGUE_SCHEMA_VERSION_1_1
} RogueSchemaVersion;

typedef enum {
    ROGUE_SCHEMA_VALIDATION_NONE = 0,
    ROGUE_SCHEMA_VALIDATION_REQUIRED = (1 << 0),
    ROGUE_SCHEMA_VALIDATION_UNIQUE = (1 << 1),
    ROGUE_SCHEMA_VALIDATION_MIN_LENGTH = (1 << 2),
    ROGUE_SCHEMA_VALIDATION_MAX_LENGTH = (1 << 3),
    ROGUE_SCHEMA_VALIDATION_MIN_VALUE = (1 << 4),
    ROGUE_SCHEMA_VALIDATION_MAX_VALUE = (1 << 5),
    ROGUE_SCHEMA_VALIDATION_PATTERN = (1 << 6),
    ROGUE_SCHEMA_VALIDATION_CUSTOM = (1 << 7)
} RogueSchemaValidationFlags;

typedef enum {
    ROGUE_SCHEMA_ERROR_NONE = 0,
    ROGUE_SCHEMA_ERROR_INVALID_TYPE,
    ROGUE_SCHEMA_ERROR_REQUIRED_FIELD_MISSING,
    ROGUE_SCHEMA_ERROR_UNKNOWN_FIELD,
    ROGUE_SCHEMA_ERROR_VALUE_TOO_SMALL,
    ROGUE_SCHEMA_ERROR_VALUE_TOO_LARGE,
    ROGUE_SCHEMA_ERROR_STRING_TOO_SHORT,
    ROGUE_SCHEMA_ERROR_STRING_TOO_LONG,
    ROGUE_SCHEMA_ERROR_PATTERN_MISMATCH,
    ROGUE_SCHEMA_ERROR_ENUM_VALUE_INVALID,
    ROGUE_SCHEMA_ERROR_ARRAY_TOO_SHORT,
    ROGUE_SCHEMA_ERROR_ARRAY_TOO_LONG,
    ROGUE_SCHEMA_ERROR_CUSTOM_VALIDATION_FAILED,
    ROGUE_SCHEMA_ERROR_CIRCULAR_REFERENCE,
    ROGUE_SCHEMA_ERROR_SCHEMA_NOT_FOUND,
    ROGUE_SCHEMA_ERROR_COUNT
} RogueSchemaErrorType;

/* ===== Forward Declarations ===== */
typedef struct RogueJsonValue RogueJsonValue;
typedef struct RogueSchemaField RogueSchemaField;
typedef struct RogueSchema RogueSchema;
typedef struct RogueSchemaRegistry RogueSchemaRegistry;
typedef struct RogueSchemaValidationResult RogueSchemaValidationResult;

/* ===== Validation Function Types ===== */
typedef bool (*RogueSchemaCustomValidator)(const RogueJsonValue* value, 
                                          const RogueSchemaField* field,
                                          void* context);

/* ===== Validation Constraints ===== */
typedef struct {
    union {
        struct {
            int64_t min_value;
            int64_t max_value;
            bool has_min;
            bool has_max;
        } integer;
        
        struct {
            double min_value;
            double max_value;
            bool has_min;
            bool has_max;
        } number;
        
        struct {
            uint32_t min_length;
            uint32_t max_length;
            char pattern[128];  /* Simple regex pattern */
            bool has_min_length;
            bool has_max_length;
            bool has_pattern;
        } string;
        
        struct {
            uint32_t min_items;
            uint32_t max_items;
            bool has_min_items;
            bool has_max_items;
            bool unique_items;
        } array;
    } constraints;
    
    /* Custom validation */
    RogueSchemaCustomValidator custom_validator;
    void* validation_context;
    
    /* Conditional validation */
    char condition_field[ROGUE_SCHEMA_MAX_NAME_LENGTH];
    char condition_value[ROGUE_SCHEMA_MAX_NAME_LENGTH];
    bool has_condition;
} RogueSchemaValidationRules;

/* ===== Schema Field Definition ===== */
typedef struct RogueSchemaField {
    char name[ROGUE_SCHEMA_MAX_NAME_LENGTH];
    char description[ROGUE_SCHEMA_MAX_DESCRIPTION_LENGTH];
    
    RogueSchemaType type;
    uint32_t validation_flags;
    RogueSchemaValidationRules validation;
    
    /* For arrays and nested objects */
    struct RogueSchema* nested_schema;
    struct RogueSchemaField* array_item_schema;
    
    /* For enums */
    char** enum_values;
    uint32_t enum_count;
    
    /* For references */
    char reference_schema[ROGUE_SCHEMA_MAX_NAME_LENGTH];
    
    /* Default value (JSON string representation) */
    char default_value[256];
    bool has_default;
    
    /* Migration information */
    uint32_t introduced_version;
    uint32_t deprecated_version;
    char migration_path[ROGUE_SCHEMA_MAX_NAME_LENGTH];
} RogueSchemaField;

/* ===== Schema Definition ===== */
typedef struct RogueSchema {
    char name[ROGUE_SCHEMA_MAX_NAME_LENGTH];
    char description[ROGUE_SCHEMA_MAX_DESCRIPTION_LENGTH];
    uint32_t version;
    
    /* Fields */
    RogueSchemaField fields[ROGUE_SCHEMA_MAX_FIELDS];
    uint32_t field_count;
    
    /* Schema composition */
    char extends[ROGUE_SCHEMA_MAX_NAME_LENGTH];  /* Inheritance */
    char includes[ROGUE_SCHEMA_MAX_DEPENDENCIES][ROGUE_SCHEMA_MAX_NAME_LENGTH];  /* Composition */
    uint32_t include_count;
    
    /* Dependencies */
    char dependencies[ROGUE_SCHEMA_MAX_DEPENDENCIES][ROGUE_SCHEMA_MAX_NAME_LENGTH];
    uint32_t dependency_count;
    
    /* Migration information */
    uint32_t schema_version;
    char migration_notes[512];
    
    /* Metadata */
    bool allow_additional_fields;
    bool strict_mode;  /* Fail on unknown fields */
} RogueSchema;

/* ===== Validation Error ===== */
typedef struct {
    RogueSchemaErrorType type;
    char field_path[ROGUE_SCHEMA_MAX_PATH_LENGTH];
    char message[256];
    uint32_t line_number;  /* For file-based validation */
    uint32_t column_number;
} RogueSchemaValidationError;

/* ===== Validation Result ===== */
typedef struct RogueSchemaValidationResult {
    bool is_valid;
    uint32_t error_count;
    RogueSchemaValidationError errors[ROGUE_SCHEMA_MAX_VALIDATION_ERRORS];
    
    /* Statistics */
    uint32_t fields_validated;
    uint32_t warnings_count;
} RogueSchemaValidationResult;

/* ===== Schema Registry ===== */
typedef struct RogueSchemaRegistry {
    RogueSchema* schemas;
    uint32_t schema_count;
    uint32_t schema_capacity;
    
    /* Schema lookup cache */
    void* schema_cache;  /* Hash table for fast lookups */
    
    /* Version management */
    uint32_t registry_version;
    
    /* Migration tracking */
    bool migration_mode;
    uint32_t target_version;
} RogueSchemaRegistry;

/* ===== Core Schema System API ===== */

/* Registry Management */
bool rogue_schema_registry_init(RogueSchemaRegistry* registry);
void rogue_schema_registry_shutdown(RogueSchemaRegistry* registry);

/* Schema Registration */
bool rogue_schema_register(RogueSchemaRegistry* registry, const RogueSchema* schema);
const RogueSchema* rogue_schema_find(const RogueSchemaRegistry* registry, const char* name);
bool rogue_schema_exists(const RogueSchemaRegistry* registry, const char* name);

/* Schema Composition */
bool rogue_schema_resolve_inheritance(RogueSchemaRegistry* registry, RogueSchema* schema);
bool rogue_schema_resolve_includes(RogueSchemaRegistry* registry, RogueSchema* schema);
bool rogue_schema_validate_dependencies(const RogueSchemaRegistry* registry, const RogueSchema* schema);

/* Schema Validation */
bool rogue_schema_validate_json(const RogueSchema* schema, const RogueJsonValue* json, 
                               RogueSchemaValidationResult* result);
bool rogue_schema_validate_field(const RogueSchemaField* field, const RogueJsonValue* value,
                                const char* field_path, RogueSchemaValidationResult* result);

/* Schema Versioning & Migration */
bool rogue_schema_migrate(RogueSchemaRegistry* registry, const char* schema_name,
                         uint32_t from_version, uint32_t to_version,
                         RogueJsonValue* json);
bool rogue_schema_check_migration_needed(const RogueSchema* schema, const RogueJsonValue* json);

/* Schema Documentation */
bool rogue_schema_generate_docs(const RogueSchema* schema, char* output_buffer, size_t buffer_size);
bool rogue_schema_export_json_schema(const RogueSchema* schema, char* output_buffer, size_t buffer_size);

/* Schema Analysis */
bool rogue_schema_analyze_coverage(const RogueSchema* schema, const RogueJsonValue* json,
                                  uint32_t* fields_covered, uint32_t* fields_missing);
bool rogue_schema_detect_circular_references(const RogueSchemaRegistry* registry, const RogueSchema* schema);

/* Utility Functions */
const char* rogue_schema_error_to_string(RogueSchemaErrorType error_type);
const char* rogue_schema_type_to_string(RogueSchemaType type);
bool rogue_schema_field_is_required(const RogueSchemaField* field);

/* Schema Builder Helper Functions */
RogueSchemaField* rogue_schema_add_field(RogueSchema* schema, const char* name, RogueSchemaType type);
void rogue_schema_field_set_required(RogueSchemaField* field, bool required);
void rogue_schema_field_set_description(RogueSchemaField* field, const char* description);
void rogue_schema_field_set_default(RogueSchemaField* field, const char* default_value);
void rogue_schema_field_set_range(RogueSchemaField* field, int64_t min, int64_t max);
void rogue_schema_field_set_string_length(RogueSchemaField* field, uint32_t min, uint32_t max);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_JSON_SCHEMA_H */
