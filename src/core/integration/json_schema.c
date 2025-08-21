/* Windows compatibility */
#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS
#endif

#include "json_schema.h"
#include "../json_parser.h"
#include "../../util/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Windows compatibility */
#ifdef _WIN32
    #define strdup _strdup
#endif

/* ===== Internal Helper Functions ===== */
static void add_validation_error(RogueSchemaValidationResult* result, 
                                RogueSchemaErrorType type,
                                const char* field_path, 
                                const char* message) {
    if (!result || result->error_count >= ROGUE_SCHEMA_MAX_VALIDATION_ERRORS) {
        return;
    }
    
    RogueSchemaValidationError* error = &result->errors[result->error_count++];
    error->type = type;
    strncpy(error->field_path, field_path, sizeof(error->field_path) - 1);
    error->field_path[sizeof(error->field_path) - 1] = '\0';
    strncpy(error->message, message, sizeof(error->message) - 1);
    error->message[sizeof(error->message) - 1] = '\0';
    error->line_number = 0;
    error->column_number = 0;
    
    result->is_valid = false;
}

static const RogueJsonValue* json_schema_object_get(const RogueJsonValue* object, const char* key) {
    if (!object || object->type != JSON_OBJECT) {
        return NULL;
    }
    
    for (size_t i = 0; i < object->data.object_value.count; i++) {
        if (strcmp(object->data.object_value.keys[i], key) == 0) {
            return object->data.object_value.values[i];
        }
    }
    
    return NULL;
}

static bool json_value_matches_type(const RogueJsonValue* value, RogueSchemaType schema_type) {
    if (!value) return false;
    
    switch (schema_type) {
        case ROGUE_SCHEMA_TYPE_NULL:
            return value->type == JSON_NULL;
        case ROGUE_SCHEMA_TYPE_BOOLEAN:
            return value->type == JSON_BOOLEAN;
        case ROGUE_SCHEMA_TYPE_INTEGER:
            return value->type == JSON_INTEGER;
        case ROGUE_SCHEMA_TYPE_NUMBER:
            return value->type == JSON_NUMBER || value->type == JSON_INTEGER;
        case ROGUE_SCHEMA_TYPE_STRING:
            return value->type == JSON_STRING;
        case ROGUE_SCHEMA_TYPE_ARRAY:
            return value->type == JSON_ARRAY;
        case ROGUE_SCHEMA_TYPE_OBJECT:
            return value->type == JSON_OBJECT;
        default:
            return false;
    }
}

static bool validate_string_constraints(const RogueJsonValue* value, 
                                       const RogueSchemaValidationRules* rules,
                                       const char* field_path,
                                       RogueSchemaValidationResult* result) {
    if (value->type != JSON_STRING) {
        return true; /* Type validation handled elsewhere */
    }
    
    const char* str = value->data.string_value;
    size_t len = str ? strlen(str) : 0;
    
    if (rules->constraints.string.has_min_length && 
        len < rules->constraints.string.min_length) {
        char msg[256];
        snprintf(msg, sizeof(msg), "String too short (got %zu, min %u)", 
                len, rules->constraints.string.min_length);
        add_validation_error(result, ROGUE_SCHEMA_ERROR_STRING_TOO_SHORT, field_path, msg);
        return false;
    }
    
    if (rules->constraints.string.has_max_length && 
        len > rules->constraints.string.max_length) {
        char msg[256];
        snprintf(msg, sizeof(msg), "String too long (got %zu, max %u)", 
                len, rules->constraints.string.max_length);
        add_validation_error(result, ROGUE_SCHEMA_ERROR_STRING_TOO_LONG, field_path, msg);
        return false;
    }
    
    /* Simple pattern matching - in a real implementation would use regex */
    if (rules->constraints.string.has_pattern && str) {
        /* Very basic pattern matching - just check if pattern is a substring */
        if (strstr(str, rules->constraints.string.pattern) == NULL) {
            char msg[256];
            snprintf(msg, sizeof(msg), "String does not match pattern '%s'", 
                    rules->constraints.string.pattern);
            add_validation_error(result, ROGUE_SCHEMA_ERROR_PATTERN_MISMATCH, field_path, msg);
            return false;
        }
    }
    
    return true;
}

static bool validate_integer_constraints(const RogueJsonValue* value, 
                                        const RogueSchemaValidationRules* rules,
                                        const char* field_path,
                                        RogueSchemaValidationResult* result) {
    if (value->type != JSON_INTEGER) {
        return true; /* Type validation handled elsewhere */
    }
    
    int64_t val = value->data.integer_value;
    
    if (rules->constraints.integer.has_min && val < rules->constraints.integer.min_value) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Integer too small (got %lld, min %lld)", 
                (long long)val, (long long)rules->constraints.integer.min_value);
        add_validation_error(result, ROGUE_SCHEMA_ERROR_VALUE_TOO_SMALL, field_path, msg);
        return false;
    }
    
    if (rules->constraints.integer.has_max && val > rules->constraints.integer.max_value) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Integer too large (got %lld, max %lld)", 
                (long long)val, (long long)rules->constraints.integer.max_value);
        add_validation_error(result, ROGUE_SCHEMA_ERROR_VALUE_TOO_LARGE, field_path, msg);
        return false;
    }
    
    return true;
}

/* ===== Registry Management Implementation ===== */

bool rogue_schema_registry_init(RogueSchemaRegistry* registry) {
    if (!registry) {
        ROGUE_LOG_ERROR("Schema registry is NULL");
        return false;
    }
    
    memset(registry, 0, sizeof(RogueSchemaRegistry));
    
    /* Initialize with some reasonable capacity */
    registry->schema_capacity = 32;
    registry->schemas = calloc(registry->schema_capacity, sizeof(RogueSchema));
    if (!registry->schemas) {
        ROGUE_LOG_ERROR("Failed to allocate schema array");
        return false;
    }
    
    registry->registry_version = ROGUE_SCHEMA_VERSION_CURRENT;
    registry->schema_cache = NULL; /* Simple implementation without hash table */
    
    ROGUE_LOG_INFO("JSON schema registry initialized");
    return true;
}

void rogue_schema_registry_shutdown(RogueSchemaRegistry* registry) {
    if (!registry) {
        return;
    }
    
    /* Free allocated schemas */
    for (uint32_t i = 0; i < registry->schema_count; i++) {
        RogueSchema* schema = &registry->schemas[i];
        
        /* Free enum values in fields */
        for (uint32_t j = 0; j < schema->field_count; j++) {
            RogueSchemaField* field = &schema->fields[j];
            if (field->enum_values) {
                for (uint32_t k = 0; k < field->enum_count; k++) {
                    free(field->enum_values[k]);
                }
                free(field->enum_values);
            }
        }
    }
    
    free(registry->schemas);
    memset(registry, 0, sizeof(RogueSchemaRegistry));
    
    ROGUE_LOG_INFO("JSON schema registry shutdown complete");
}

/* ===== Schema Registration Implementation ===== */

bool rogue_schema_register(RogueSchemaRegistry* registry, const RogueSchema* schema) {
    if (!registry || !schema) {
        ROGUE_LOG_ERROR("Invalid parameters for schema registration");
        return false;
    }
    
    /* Check if schema already exists */
    if (rogue_schema_exists(registry, schema->name)) {
        ROGUE_LOG_ERROR("Schema '%s' already registered", schema->name);
        return false;
    }
    
    /* Expand capacity if needed */
    if (registry->schema_count >= registry->schema_capacity) {
        uint32_t new_capacity = registry->schema_capacity * 2;
        RogueSchema* new_schemas = realloc(registry->schemas, 
                                          new_capacity * sizeof(RogueSchema));
        if (!new_schemas) {
            ROGUE_LOG_ERROR("Failed to expand schema registry capacity");
            return false;
        }
        
        registry->schemas = new_schemas;
        registry->schema_capacity = new_capacity;
    }
    
    /* Copy schema */
    registry->schemas[registry->schema_count] = *schema;
    registry->schema_count++;
    
    ROGUE_LOG_INFO("Registered schema '%s' (version %u)", schema->name, schema->version);
    return true;
}

const RogueSchema* rogue_schema_find(const RogueSchemaRegistry* registry, const char* name) {
    if (!registry || !name) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < registry->schema_count; i++) {
        if (strcmp(registry->schemas[i].name, name) == 0) {
            return &registry->schemas[i];
        }
    }
    
    return NULL;
}

bool rogue_schema_exists(const RogueSchemaRegistry* registry, const char* name) {
    return rogue_schema_find(registry, name) != NULL;
}

/* ===== Validation Implementation ===== */

bool rogue_schema_validate_field(const RogueSchemaField* field, const RogueJsonValue* value,
                                const char* field_path, RogueSchemaValidationResult* result) {
    if (!field || !result) {
        return false;
    }
    
    result->fields_validated++;
    
    /* Check if field is required */
    if ((field->validation_flags & ROGUE_SCHEMA_VALIDATION_REQUIRED) && !value) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Required field '%s' is missing", field->name);
        add_validation_error(result, ROGUE_SCHEMA_ERROR_REQUIRED_FIELD_MISSING, field_path, msg);
        return false;
    }
    
    /* If value is null and field is not required, that's OK */
    if (!value) {
        return true;
    }
    
    /* Check type compatibility */
    if (!json_value_matches_type(value, field->type)) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Type mismatch: expected %s, got %d", 
                rogue_schema_type_to_string(field->type), value->type);
        add_validation_error(result, ROGUE_SCHEMA_ERROR_INVALID_TYPE, field_path, msg);
        return false;
    }
    
    /* Validate constraints based on type */
    bool valid = true;
    
    switch (field->type) {
        case ROGUE_SCHEMA_TYPE_STRING:
            valid = validate_string_constraints(value, &field->validation, field_path, result);
            break;
            
        case ROGUE_SCHEMA_TYPE_INTEGER:
            valid = validate_integer_constraints(value, &field->validation, field_path, result);
            break;
            
        case ROGUE_SCHEMA_TYPE_ARRAY:
            if (value->type == JSON_ARRAY) {
                size_t count = value->data.array_value.count;
                if (field->validation.constraints.array.has_min_items && 
                    count < field->validation.constraints.array.min_items) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Array too short (got %zu, min %u)", 
                            count, field->validation.constraints.array.min_items);
                    add_validation_error(result, ROGUE_SCHEMA_ERROR_ARRAY_TOO_SHORT, field_path, msg);
                    valid = false;
                }
                
                if (field->validation.constraints.array.has_max_items && 
                    count > field->validation.constraints.array.max_items) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Array too long (got %zu, max %u)", 
                            count, field->validation.constraints.array.max_items);
                    add_validation_error(result, ROGUE_SCHEMA_ERROR_ARRAY_TOO_LONG, field_path, msg);
                    valid = false;
                }
            }
            break;
            
        case ROGUE_SCHEMA_TYPE_ENUM:
            if (value->type == JSON_STRING && field->enum_values) {
                bool found = false;
                for (uint32_t i = 0; i < field->enum_count; i++) {
                    if (strcmp(value->data.string_value, field->enum_values[i]) == 0) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Invalid enum value '%s'", value->data.string_value);
                    add_validation_error(result, ROGUE_SCHEMA_ERROR_ENUM_VALUE_INVALID, field_path, msg);
                    valid = false;
                }
            }
            break;
            
        default:
            /* Other types don't have specific constraint validation yet */
            break;
    }
    
    /* Custom validation */
    if (valid && field->validation.custom_validator) {
        if (!field->validation.custom_validator(value, field, field->validation.validation_context)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Custom validation failed for field '%s'", field->name);
            add_validation_error(result, ROGUE_SCHEMA_ERROR_CUSTOM_VALIDATION_FAILED, field_path, msg);
            valid = false;
        }
    }
    
    return valid;
}

bool rogue_schema_validate_json(const RogueSchema* schema, const RogueJsonValue* json, 
                               RogueSchemaValidationResult* result) {
    if (!schema || !json || !result) {
        return false;
    }
    
    /* Initialize result */
    memset(result, 0, sizeof(RogueSchemaValidationResult));
    result->is_valid = true;
    
    /* JSON must be an object for schema validation */
    if (json->type != JSON_OBJECT) {
        add_validation_error(result, ROGUE_SCHEMA_ERROR_INVALID_TYPE, "", "Root value must be an object");
        return false;
    }
    
    /* Validate each schema field */
    for (uint32_t i = 0; i < schema->field_count; i++) {
        const RogueSchemaField* field = &schema->fields[i];
        const RogueJsonValue* value = json_schema_object_get(json, field->name);
        
        char field_path[ROGUE_SCHEMA_MAX_PATH_LENGTH];
        snprintf(field_path, sizeof(field_path), "%s", field->name);
        
        rogue_schema_validate_field(field, value, field_path, result);
    }
    
    /* Check for unknown fields if in strict mode */
    if (schema->strict_mode && json->type == JSON_OBJECT) {
        for (size_t i = 0; i < json->data.object_value.count; i++) {
            const char* key = json->data.object_value.keys[i];
            
            /* Check if this key is defined in the schema */
            bool found = false;
            for (uint32_t j = 0; j < schema->field_count; j++) {
                if (strcmp(schema->fields[j].name, key) == 0) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                char msg[256];
                snprintf(msg, sizeof(msg), "Unknown field '%s' not allowed in strict mode", key);
                add_validation_error(result, ROGUE_SCHEMA_ERROR_UNKNOWN_FIELD, key, msg);
            }
        }
    }
    
    return result->is_valid;
}

/* ===== Utility Functions Implementation ===== */

const char* rogue_schema_error_to_string(RogueSchemaErrorType error_type) {
    switch (error_type) {
        case ROGUE_SCHEMA_ERROR_NONE: return "No error";
        case ROGUE_SCHEMA_ERROR_INVALID_TYPE: return "Invalid type";
        case ROGUE_SCHEMA_ERROR_REQUIRED_FIELD_MISSING: return "Required field missing";
        case ROGUE_SCHEMA_ERROR_UNKNOWN_FIELD: return "Unknown field";
        case ROGUE_SCHEMA_ERROR_VALUE_TOO_SMALL: return "Value too small";
        case ROGUE_SCHEMA_ERROR_VALUE_TOO_LARGE: return "Value too large";
        case ROGUE_SCHEMA_ERROR_STRING_TOO_SHORT: return "String too short";
        case ROGUE_SCHEMA_ERROR_STRING_TOO_LONG: return "String too long";
        case ROGUE_SCHEMA_ERROR_PATTERN_MISMATCH: return "Pattern mismatch";
        case ROGUE_SCHEMA_ERROR_ENUM_VALUE_INVALID: return "Invalid enum value";
        case ROGUE_SCHEMA_ERROR_ARRAY_TOO_SHORT: return "Array too short";
        case ROGUE_SCHEMA_ERROR_ARRAY_TOO_LONG: return "Array too long";
        case ROGUE_SCHEMA_ERROR_CUSTOM_VALIDATION_FAILED: return "Custom validation failed";
        case ROGUE_SCHEMA_ERROR_CIRCULAR_REFERENCE: return "Circular reference";
        case ROGUE_SCHEMA_ERROR_SCHEMA_NOT_FOUND: return "Schema not found";
        default: return "Unknown error";
    }
}

const char* rogue_schema_type_to_string(RogueSchemaType type) {
    switch (type) {
        case ROGUE_SCHEMA_TYPE_NULL: return "null";
        case ROGUE_SCHEMA_TYPE_BOOLEAN: return "boolean";
        case ROGUE_SCHEMA_TYPE_INTEGER: return "integer";
        case ROGUE_SCHEMA_TYPE_NUMBER: return "number";
        case ROGUE_SCHEMA_TYPE_STRING: return "string";
        case ROGUE_SCHEMA_TYPE_ARRAY: return "array";
        case ROGUE_SCHEMA_TYPE_OBJECT: return "object";
        case ROGUE_SCHEMA_TYPE_ENUM: return "enum";
        case ROGUE_SCHEMA_TYPE_REFERENCE: return "reference";
        default: return "unknown";
    }
}

bool rogue_schema_field_is_required(const RogueSchemaField* field) {
    return field && (field->validation_flags & ROGUE_SCHEMA_VALIDATION_REQUIRED);
}

/* ===== Schema Builder Helper Functions ===== */

RogueSchemaField* rogue_schema_add_field(RogueSchema* schema, const char* name, RogueSchemaType type) {
    if (!schema || !name || schema->field_count >= ROGUE_SCHEMA_MAX_FIELDS) {
        return NULL;
    }
    
    RogueSchemaField* field = &schema->fields[schema->field_count++];
    memset(field, 0, sizeof(RogueSchemaField));
    
    strncpy(field->name, name, sizeof(field->name) - 1);
    field->name[sizeof(field->name) - 1] = '\0';
    
    field->type = type;
    field->introduced_version = schema->version;
    
    return field;
}

void rogue_schema_field_set_required(RogueSchemaField* field, bool required) {
    if (!field) return;
    
    if (required) {
        field->validation_flags |= ROGUE_SCHEMA_VALIDATION_REQUIRED;
    } else {
        field->validation_flags &= ~ROGUE_SCHEMA_VALIDATION_REQUIRED;
    }
}

void rogue_schema_field_set_description(RogueSchemaField* field, const char* description) {
    if (!field || !description) return;
    
    strncpy(field->description, description, sizeof(field->description) - 1);
    field->description[sizeof(field->description) - 1] = '\0';
}

void rogue_schema_field_set_default(RogueSchemaField* field, const char* default_value) {
    if (!field || !default_value) return;
    
    strncpy(field->default_value, default_value, sizeof(field->default_value) - 1);
    field->default_value[sizeof(field->default_value) - 1] = '\0';
    field->has_default = true;
}

void rogue_schema_field_set_range(RogueSchemaField* field, int64_t min, int64_t max) {
    if (!field) return;
    
    field->validation_flags |= ROGUE_SCHEMA_VALIDATION_MIN_VALUE | ROGUE_SCHEMA_VALIDATION_MAX_VALUE;
    field->validation.constraints.integer.min_value = min;
    field->validation.constraints.integer.max_value = max;
    field->validation.constraints.integer.has_min = true;
    field->validation.constraints.integer.has_max = true;
}

void rogue_schema_field_set_string_length(RogueSchemaField* field, uint32_t min, uint32_t max) {
    if (!field) return;
    
    field->validation_flags |= ROGUE_SCHEMA_VALIDATION_MIN_LENGTH | ROGUE_SCHEMA_VALIDATION_MAX_LENGTH;
    field->validation.constraints.string.min_length = min;
    field->validation.constraints.string.max_length = max;
    field->validation.constraints.string.has_min_length = true;
    field->validation.constraints.string.has_max_length = true;
}

/* ===== Stub implementations for unimplemented functions ===== */

bool rogue_schema_resolve_inheritance(RogueSchemaRegistry* registry, RogueSchema* schema) {
    (void)registry; (void)schema;
    /* TODO: Implement schema inheritance resolution */
    return true;
}

bool rogue_schema_resolve_includes(RogueSchemaRegistry* registry, RogueSchema* schema) {
    (void)registry; (void)schema;
    /* TODO: Implement schema includes resolution */
    return true;
}

bool rogue_schema_validate_dependencies(const RogueSchemaRegistry* registry, const RogueSchema* schema) {
    (void)registry; (void)schema;
    /* TODO: Implement dependency validation */
    return true;
}

bool rogue_schema_migrate(RogueSchemaRegistry* registry, const char* schema_name,
                         uint32_t from_version, uint32_t to_version,
                         RogueJsonValue* json) {
    (void)registry; (void)schema_name; (void)from_version; (void)to_version; (void)json;
    /* TODO: Implement schema migration */
    return true;
}

bool rogue_schema_check_migration_needed(const RogueSchema* schema, const RogueJsonValue* json) {
    (void)schema; (void)json;
    /* TODO: Implement migration check */
    return false;
}

bool rogue_schema_generate_docs(const RogueSchema* schema, char* output_buffer, size_t buffer_size) {
    (void)schema; (void)output_buffer; (void)buffer_size;
    /* TODO: Implement documentation generation */
    return false;
}

bool rogue_schema_export_json_schema(const RogueSchema* schema, char* output_buffer, size_t buffer_size) {
    (void)schema; (void)output_buffer; (void)buffer_size;
    /* TODO: Implement JSON schema export */
    return false;
}

bool rogue_schema_analyze_coverage(const RogueSchema* schema, const RogueJsonValue* json,
                                  uint32_t* fields_covered, uint32_t* fields_missing) {
    (void)schema; (void)json; (void)fields_covered; (void)fields_missing;
    /* TODO: Implement coverage analysis */
    return false;
}

bool rogue_schema_detect_circular_references(const RogueSchemaRegistry* registry, const RogueSchema* schema) {
    (void)registry; (void)schema;
    /* TODO: Implement circular reference detection */
    return false;
}
