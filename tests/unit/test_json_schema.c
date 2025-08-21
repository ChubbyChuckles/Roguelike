#include "../../src/core/integration/json_schema.h"
#include "../../src/core/json_parser.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test helper functions */
static void print_test_header(const char* test_name) {
    printf("Running test: %s...\n", test_name);
}

static void print_test_result(const char* test_name, bool passed) {
    printf("  %s\n", passed ? "PASS" : "FAIL");
    if (!passed) {
        exit(1); /* Fail fast for debugging */
    }
}

/* Simple JSON value creation helpers for testing */
static RogueJsonValue* create_json_string(const char* value) {
    return json_create_string(value);
}

static RogueJsonValue* create_json_integer(int64_t value) {
    return json_create_integer(value);
}

static RogueJsonValue* create_json_object(void) {
    return json_create_object();
}

static void json_object_add_string(RogueJsonValue* object, const char* key, const char* value) {
    RogueJsonValue* str_value = json_create_string(value);
    if (str_value) {
        json_object_set(object, key, str_value);
    }
}

static void json_object_add_integer(RogueJsonValue* object, const char* key, int64_t value) {
    RogueJsonValue* int_value = json_create_integer(value);
    if (int_value) {
        json_object_set(object, key, int_value);
    }
}

static void free_json_value(RogueJsonValue* json) {
    json_free(json);
}

/* ===== Test Functions ===== */

static void test_schema_registry_initialization(void) {
    print_test_header("Schema Registry Initialization");
    
    RogueSchemaRegistry registry;
    
    /* Test successful initialization */
    bool result = rogue_schema_registry_init(&registry);
    assert(result == true);
    assert(registry.schema_count == 0);
    assert(registry.schema_capacity == 32);
    assert(registry.schemas != NULL);
    assert(registry.registry_version == ROGUE_SCHEMA_VERSION_CURRENT);
    
    /* Test cleanup */
    rogue_schema_registry_shutdown(&registry);
    assert(registry.schemas == NULL);
    assert(registry.schema_count == 0);
    
    /* Test NULL parameter handling */
    result = rogue_schema_registry_init(NULL);
    assert(result == false);
    
    print_test_result("Schema Registry Initialization", true);
}

static void test_schema_registration(void) {
    print_test_header("Schema Registration");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create a simple schema */
    RogueSchema schema = {0};
    strncpy(schema.name, "TestSchema", sizeof(schema.name) - 1);
    strncpy(schema.description, "A test schema", sizeof(schema.description) - 1);
    schema.version = 1;
    schema.strict_mode = true;
    
    /* Add some fields */
    RogueSchemaField* field1 = rogue_schema_add_field(&schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    assert(field1 != NULL);
    rogue_schema_field_set_required(field1, true);
    rogue_schema_field_set_description(field1, "The name field");
    rogue_schema_field_set_string_length(field1, 1, 50);
    
    RogueSchemaField* field2 = rogue_schema_add_field(&schema, "age", ROGUE_SCHEMA_TYPE_INTEGER);
    assert(field2 != NULL);
    rogue_schema_field_set_range(field2, 0, 120);
    rogue_schema_field_set_default(field2, "0");
    
    assert(schema.field_count == 2);
    
    /* Register the schema */
    bool result = rogue_schema_register(&registry, &schema);
    assert(result == true);
    assert(registry.schema_count == 1);
    
    /* Test finding the schema */
    const RogueSchema* found_schema = rogue_schema_find(&registry, "TestSchema");
    assert(found_schema != NULL);
    assert(strcmp(found_schema->name, "TestSchema") == 0);
    assert(found_schema->field_count == 2);
    
    /* Test schema exists check */
    assert(rogue_schema_exists(&registry, "TestSchema") == true);
    assert(rogue_schema_exists(&registry, "NonExistent") == false);
    
    /* Test duplicate registration */
    result = rogue_schema_register(&registry, &schema);
    assert(result == false);
    assert(registry.schema_count == 1);
    
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Schema Registration", true);
}

static void test_field_validation_required_fields(void) {
    print_test_header("Field Validation - Required Fields");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create schema with required field */
    RogueSchema schema = {0};
    strncpy(schema.name, "RequiredFieldTest", sizeof(schema.name) - 1);
    schema.version = 1;
    
    RogueSchemaField* field = rogue_schema_add_field(&schema, "required_field", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(field, true);
    
    rogue_schema_register(&registry, &schema);
    
    /* Test validation with missing required field */
    RogueJsonValue* json = create_json_object();
    json_object_add_string(json, "other_field", "value");
    
    RogueSchemaValidationResult result;
    bool valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_REQUIRED_FIELD_MISSING);
    assert(strcmp(result.errors[0].field_path, "required_field") == 0);
    
    /* Test validation with required field present */
    json_object_add_string(json, "required_field", "present");
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    assert(valid == true);
    assert(result.error_count == 0);
    
    free_json_value(json);
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Field Validation - Required Fields", true);
}

static void test_field_validation_type_checking(void) {
    print_test_header("Field Validation - Type Checking");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create schema with different field types */
    RogueSchema schema = {0};
    strncpy(schema.name, "TypeTest", sizeof(schema.name) - 1);
    schema.version = 1;
    
    rogue_schema_add_field(&schema, "string_field", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_add_field(&schema, "integer_field", ROGUE_SCHEMA_TYPE_INTEGER);
    
    rogue_schema_register(&registry, &schema);
    
    /* Test validation with correct types */
    RogueJsonValue* json = create_json_object();
    json_object_add_string(json, "string_field", "hello");
    json_object_add_integer(json, "integer_field", 42);
    
    RogueSchemaValidationResult result;
    bool valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == true);
    assert(result.error_count == 0);
    assert(result.fields_validated == 2);
    
    /* Test validation with wrong type (string for integer field) */
    free_json_value(json);
    json = create_json_object();
    json_object_add_string(json, "string_field", "hello");
    json_object_add_string(json, "integer_field", "not_a_number"); /* Wrong type */
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_INVALID_TYPE);
    
    free_json_value(json);
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Field Validation - Type Checking", true);
}

static void test_field_validation_string_constraints(void) {
    print_test_header("Field Validation - String Constraints");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create schema with string length constraints */
    RogueSchema schema = {0};
    strncpy(schema.name, "StringConstraintTest", sizeof(schema.name) - 1);
    schema.version = 1;
    
    RogueSchemaField* field = rogue_schema_add_field(&schema, "constrained_string", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_string_length(field, 5, 10); /* Min 5, Max 10 characters */
    
    rogue_schema_register(&registry, &schema);
    
    /* Test string too short */
    RogueJsonValue* json = create_json_object();
    json_object_add_string(json, "constrained_string", "hi"); /* Too short */
    
    RogueSchemaValidationResult result;
    bool valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_STRING_TOO_SHORT);
    
    /* Test string too long */
    free_json_value(json);
    json = create_json_object();
    json_object_add_string(json, "constrained_string", "this_is_way_too_long"); /* Too long */
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_STRING_TOO_LONG);
    
    /* Test valid string */
    free_json_value(json);
    json = create_json_object();
    json_object_add_string(json, "constrained_string", "perfect"); /* Just right */
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == true);
    assert(result.error_count == 0);
    
    free_json_value(json);
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Field Validation - String Constraints", true);
}

static void test_field_validation_integer_constraints(void) {
    print_test_header("Field Validation - Integer Constraints");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create schema with integer range constraints */
    RogueSchema schema = {0};
    strncpy(schema.name, "IntegerConstraintTest", sizeof(schema.name) - 1);
    schema.version = 1;
    
    RogueSchemaField* field = rogue_schema_add_field(&schema, "constrained_int", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_range(field, 1, 100); /* Min 1, Max 100 */
    
    rogue_schema_register(&registry, &schema);
    
    /* Test integer too small */
    RogueJsonValue* json = create_json_object();
    json_object_add_integer(json, "constrained_int", 0); /* Too small */
    
    RogueSchemaValidationResult result;
    bool valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_VALUE_TOO_SMALL);
    
    /* Test integer too large */
    free_json_value(json);
    json = create_json_object();
    json_object_add_integer(json, "constrained_int", 101); /* Too large */
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_VALUE_TOO_LARGE);
    
    /* Test valid integer */
    free_json_value(json);
    json = create_json_object();
    json_object_add_integer(json, "constrained_int", 50); /* Perfect */
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == true);
    assert(result.error_count == 0);
    
    free_json_value(json);
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Field Validation - Integer Constraints", true);
}

static void test_strict_mode_validation(void) {
    print_test_header("Strict Mode Validation");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create schema in strict mode */
    RogueSchema schema = {0};
    strncpy(schema.name, "StrictModeTest", sizeof(schema.name) - 1);
    schema.version = 1;
    schema.strict_mode = true;
    
    rogue_schema_add_field(&schema, "allowed_field", ROGUE_SCHEMA_TYPE_STRING);
    
    rogue_schema_register(&registry, &schema);
    
    /* Test validation with unknown field in strict mode */
    RogueJsonValue* json = create_json_object();
    json_object_add_string(json, "allowed_field", "ok");
    json_object_add_string(json, "unknown_field", "not_allowed"); /* Should fail in strict mode */
    
    RogueSchemaValidationResult result;
    bool valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count == 1);
    assert(result.errors[0].type == ROGUE_SCHEMA_ERROR_UNKNOWN_FIELD);
    assert(strcmp(result.errors[0].field_path, "unknown_field") == 0);
    
    free_json_value(json);
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Strict Mode Validation", true);
}

static void test_helper_functions(void) {
    print_test_header("Helper Functions");
    
    /* Test error type to string conversion */
    const char* error_str = rogue_schema_error_to_string(ROGUE_SCHEMA_ERROR_INVALID_TYPE);
    assert(strcmp(error_str, "Invalid type") == 0);
    
    error_str = rogue_schema_error_to_string(ROGUE_SCHEMA_ERROR_REQUIRED_FIELD_MISSING);
    assert(strcmp(error_str, "Required field missing") == 0);
    
    /* Test schema type to string conversion */
    const char* type_str = rogue_schema_type_to_string(ROGUE_SCHEMA_TYPE_STRING);
    assert(strcmp(type_str, "string") == 0);
    
    type_str = rogue_schema_type_to_string(ROGUE_SCHEMA_TYPE_INTEGER);
    assert(strcmp(type_str, "integer") == 0);
    
    /* Test field required check */
    RogueSchemaField field = {0};
    assert(rogue_schema_field_is_required(&field) == false);
    
    rogue_schema_field_set_required(&field, true);
    assert(rogue_schema_field_is_required(&field) == true);
    
    rogue_schema_field_set_required(&field, false);
    assert(rogue_schema_field_is_required(&field) == false);
    
    print_test_result("Helper Functions", true);
}

static void test_schema_builder_functions(void) {
    print_test_header("Schema Builder Functions");
    
    RogueSchema schema = {0};
    strncpy(schema.name, "BuilderTest", sizeof(schema.name) - 1);
    schema.version = 1;
    
    /* Test adding fields */
    RogueSchemaField* field1 = rogue_schema_add_field(&schema, "test_string", ROGUE_SCHEMA_TYPE_STRING);
    assert(field1 != NULL);
    assert(strcmp(field1->name, "test_string") == 0);
    assert(field1->type == ROGUE_SCHEMA_TYPE_STRING);
    assert(schema.field_count == 1);
    
    RogueSchemaField* field2 = rogue_schema_add_field(&schema, "test_int", ROGUE_SCHEMA_TYPE_INTEGER);
    assert(field2 != NULL);
    assert(strcmp(field2->name, "test_int") == 0);
    assert(field2->type == ROGUE_SCHEMA_TYPE_INTEGER);
    assert(schema.field_count == 2);
    
    /* Test setting field properties */
    rogue_schema_field_set_description(field1, "Test description");
    assert(strcmp(field1->description, "Test description") == 0);
    
    rogue_schema_field_set_default(field1, "default_value");
    assert(strcmp(field1->default_value, "default_value") == 0);
    assert(field1->has_default == true);
    
    rogue_schema_field_set_string_length(field1, 2, 20);
    assert(field1->validation.constraints.string.min_length == 2);
    assert(field1->validation.constraints.string.max_length == 20);
    assert(field1->validation.constraints.string.has_min_length == true);
    assert(field1->validation.constraints.string.has_max_length == true);
    
    rogue_schema_field_set_range(field2, -10, 10);
    assert(field2->validation.constraints.integer.min_value == -10);
    assert(field2->validation.constraints.integer.max_value == 10);
    assert(field2->validation.constraints.integer.has_min == true);
    assert(field2->validation.constraints.integer.has_max == true);
    
    /* Test field overflow protection */
    for (int i = 0; i < ROGUE_SCHEMA_MAX_FIELDS + 10; i++) {
        char field_name[32];
        snprintf(field_name, sizeof(field_name), "field_%d", i);
        RogueSchemaField* field = rogue_schema_add_field(&schema, field_name, ROGUE_SCHEMA_TYPE_STRING);
        
        if (i < ROGUE_SCHEMA_MAX_FIELDS - 2) { /* -2 because we already added 2 fields */
            assert(field != NULL);
        } else {
            assert(field == NULL); /* Should fail when limit reached */
        }
    }
    
    assert(schema.field_count == ROGUE_SCHEMA_MAX_FIELDS);
    
    print_test_result("Schema Builder Functions", true);
}

static void test_comprehensive_validation(void) {
    print_test_header("Comprehensive Validation");
    
    RogueSchemaRegistry registry;
    rogue_schema_registry_init(&registry);
    
    /* Create a comprehensive schema for an item definition */
    RogueSchema schema = {0};
    strncpy(schema.name, "ItemSchema", sizeof(schema.name) - 1);
    strncpy(schema.description, "Schema for game item definitions", sizeof(schema.description) - 1);
    schema.version = 1;
    schema.strict_mode = true;
    
    /* Add various field types with constraints */
    RogueSchemaField* name_field = rogue_schema_add_field(&schema, "name", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_required(name_field, true);
    rogue_schema_field_set_description(name_field, "Item name");
    rogue_schema_field_set_string_length(name_field, 1, 50);
    
    RogueSchemaField* id_field = rogue_schema_add_field(&schema, "id", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_required(id_field, true);
    rogue_schema_field_set_description(id_field, "Unique item ID");
    rogue_schema_field_set_range(id_field, 1, 99999);
    
    RogueSchemaField* description_field = rogue_schema_add_field(&schema, "description", ROGUE_SCHEMA_TYPE_STRING);
    rogue_schema_field_set_description(description_field, "Item description");
    rogue_schema_field_set_string_length(description_field, 0, 500);
    
    RogueSchemaField* value_field = rogue_schema_add_field(&schema, "value", ROGUE_SCHEMA_TYPE_INTEGER);
    rogue_schema_field_set_description(value_field, "Item value in gold");
    rogue_schema_field_set_range(value_field, 0, 1000000);
    rogue_schema_field_set_default(value_field, "0");
    
    rogue_schema_register(&registry, &schema);
    
    /* Test valid item JSON */
    RogueJsonValue* json = create_json_object();
    json_object_add_string(json, "name", "Steel Sword");
    json_object_add_integer(json, "id", 1001);
    json_object_add_string(json, "description", "A sturdy steel sword");
    json_object_add_integer(json, "value", 150);
    
    RogueSchemaValidationResult result;
    bool valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == true);
    assert(result.error_count == 0);
    assert(result.fields_validated == 4);
    
    /* Test invalid item JSON (multiple errors) */
    free_json_value(json);
    json = create_json_object();
    json_object_add_string(json, "name", ""); /* Too short */
    /* Missing required 'id' field */
    json_object_add_string(json, "description", "A" /* Create very long string */);
    /* Add characters to make description too long */
    char long_desc[600];
    memset(long_desc, 'A', 599);
    long_desc[599] = '\0';
    free_json_value(json);
    json = create_json_object();
    json_object_add_string(json, "name", "");
    json_object_add_string(json, "description", long_desc);
    json_object_add_integer(json, "value", -100); /* Too small */
    json_object_add_string(json, "unknown_field", "not_allowed"); /* Unknown field in strict mode */
    
    valid = rogue_schema_validate_json(&schema, json, &result);
    
    assert(valid == false);
    assert(result.error_count >= 4); /* Should have multiple errors */
    
    /* Check that we have the expected error types */
    bool found_string_too_short = false;
    bool found_required_missing = false;
    bool found_string_too_long = false;
    bool found_value_too_small = false;
    bool found_unknown_field = false;
    
    for (uint32_t i = 0; i < result.error_count; i++) {
        switch (result.errors[i].type) {
            case ROGUE_SCHEMA_ERROR_STRING_TOO_SHORT:
                found_string_too_short = true;
                break;
            case ROGUE_SCHEMA_ERROR_REQUIRED_FIELD_MISSING:
                found_required_missing = true;
                break;
            case ROGUE_SCHEMA_ERROR_STRING_TOO_LONG:
                found_string_too_long = true;
                break;
            case ROGUE_SCHEMA_ERROR_VALUE_TOO_SMALL:
                found_value_too_small = true;
                break;
            case ROGUE_SCHEMA_ERROR_UNKNOWN_FIELD:
                found_unknown_field = true;
                break;
            default:
                break;
        }
    }
    
    assert(found_string_too_short == true);
    assert(found_required_missing == true);
    assert(found_string_too_long == true);
    assert(found_value_too_small == true);
    assert(found_unknown_field == true);
    
    free_json_value(json);
    rogue_schema_registry_shutdown(&registry);
    print_test_result("Comprehensive Validation", true);
}

/* ===== Main Test Runner ===== */

int main(void) {
    printf("=== JSON Schema Unit Tests ===\n\n");
    
    test_schema_registry_initialization();
    test_schema_registration();
    test_field_validation_required_fields();
    test_field_validation_type_checking();
    test_field_validation_string_constraints();
    test_field_validation_integer_constraints();
    test_strict_mode_validation();
    test_helper_functions();
    test_schema_builder_functions();
    test_comprehensive_validation();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: 10\n");
    printf("Tests passed: 10\n");
    printf("Tests failed: 0\n");
    printf("All tests PASSED!\n");
    
    return 0;
}
