/**
 * @file json_parser.c
 * @brief JSON data structure manipulation library.
 * @details This module provides functions for creating, manipulating, and managing
 * JSON data structures including objects, arrays, and primitive values.
 */

#include "json_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Windows compatibility for strdup */
#ifdef _WIN32
#define strdup _strdup
#endif

/* ===== JSON Value Creation Functions ===== */

/**
 * @brief Creates a JSON null value.
 * @return Pointer to the new JSON value, or NULL on allocation failure.
 * @details Allocates and initializes a new JSON value of type null.
 */
RogueJsonValue* json_create_null(void)
{
    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_NULL;
    return json;
}

/**
 * @brief Creates a JSON boolean value.
 * @param value The boolean value to store.
 * @return Pointer to the new JSON value, or NULL on allocation failure.
 * @details Allocates and initializes a new JSON value with the specified boolean value.
 */
RogueJsonValue* json_create_boolean(bool value)
{
    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_BOOLEAN;
    json->data.boolean_value = value;
    return json;
}

/**
 * @brief Creates a JSON integer value.
 * @param value The integer value to store.
 * @return Pointer to the new JSON value, or NULL on allocation failure.
 * @details Allocates and initializes a new JSON value with the specified 64-bit integer value.
 */
RogueJsonValue* json_create_integer(int64_t value)
{
    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_INTEGER;
    json->data.integer_value = value;
    return json;
}

/**
 * @brief Creates a JSON number (floating point) value.
 * @param value The number value to store.
 * @return Pointer to the new JSON value, or NULL on allocation failure.
 * @details Allocates and initializes a new JSON value with the specified double-precision value.
 */
RogueJsonValue* json_create_number(double value)
{
    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_NUMBER;
    json->data.number_value = value;
    return json;
}

/**
 * @brief Creates a JSON string value.
 * @param value The string value to store (will be duplicated).
 * @return Pointer to the new JSON value, or NULL on allocation failure.
 * @details Allocates and initializes a new JSON value with a copy of the specified string.
 */
RogueJsonValue* json_create_string(const char* value)
{
    if (!value)
        return NULL;

    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_STRING;
    json->data.string_value = strdup(value);
    if (!json->data.string_value)
    {
        free(json);
        return NULL;
    }

    return json;
}

/**
 * @brief Creates an empty JSON array.
 * @return Pointer to the new JSON array, or NULL on allocation failure.
 * @details Allocates and initializes a new empty JSON array value.
 */
RogueJsonValue* json_create_array(void)
{
    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_ARRAY;
    json->data.array_value.items = NULL;
    json->data.array_value.count = 0;
    return json;
}

/**
 * @brief Creates an empty JSON object.
 * @return Pointer to the new JSON object, or NULL on allocation failure.
 * @details Allocates and initializes a new empty JSON object value.
 */
RogueJsonValue* json_create_object(void)
{
    RogueJsonValue* json = calloc(1, sizeof(RogueJsonValue));
    if (!json)
        return NULL;

    json->type = JSON_OBJECT;
    json->data.object_value.keys = NULL;
    json->data.object_value.values = NULL;
    json->data.object_value.count = 0;
    return json;
}

/* ===== JSON Manipulation Functions ===== */

/**
 * @brief Adds an item to a JSON array.
 * @param array Pointer to the JSON array value.
 * @param item Pointer to the JSON value to add.
 * @return true on success, false on failure.
 * @details Appends the item to the end of the array. The array takes ownership of the item.
 */
bool json_array_add(RogueJsonValue* array, RogueJsonValue* item)
{
    if (!array || array->type != JSON_ARRAY || !item)
    {
        return false;
    }

    size_t new_count = array->data.array_value.count + 1;
    RogueJsonValue** new_items =
        realloc(array->data.array_value.items, new_count * sizeof(RogueJsonValue*));
    if (!new_items)
    {
        return false;
    }

    new_items[new_count - 1] = item;
    array->data.array_value.items = new_items;
    array->data.array_value.count = new_count;

    return true;
}

/**
 * @brief Sets a key-value pair in a JSON object.
 * @param object Pointer to the JSON object value.
 * @param key The key string (will be duplicated).
 * @param value Pointer to the JSON value to set.
 * @return true on success, false on failure.
 * @details If the key already exists, replaces the value. Otherwise adds a new key-value pair.
 * The object takes ownership of both the key and value.
 */
bool json_object_set(RogueJsonValue* object, const char* key, RogueJsonValue* value)
{
    if (!object || object->type != JSON_OBJECT || !key || !value)
    {
        return false;
    }

    /* Check if key already exists */
    for (size_t i = 0; i < object->data.object_value.count; i++)
    {
        if (strcmp(object->data.object_value.keys[i], key) == 0)
        {
            /* Replace existing value */
            json_free(object->data.object_value.values[i]);
            object->data.object_value.values[i] = value;
            return true;
        }
    }

    /* Add new key-value pair */
    size_t new_count = object->data.object_value.count + 1;

    char** new_keys = realloc(object->data.object_value.keys, new_count * sizeof(char*));
    if (!new_keys)
        return false;

    RogueJsonValue** new_values =
        realloc(object->data.object_value.values, new_count * sizeof(RogueJsonValue*));
    if (!new_values)
    {
        free(new_keys);
        return false;
    }

    new_keys[new_count - 1] = strdup(key);
    if (!new_keys[new_count - 1])
    {
        free(new_keys);
        free(new_values);
        return false;
    }

    new_values[new_count - 1] = value;

    object->data.object_value.keys = new_keys;
    object->data.object_value.values = new_values;
    object->data.object_value.count = new_count;

    return true;
}

/**
 * @brief Gets a value from a JSON object by key.
 * @param object Pointer to the JSON object value.
 * @param key The key to look up.
 * @return Pointer to the JSON value, or NULL if not found.
 * @details Returns the value associated with the key, or NULL if the key doesn't exist.
 */
RogueJsonValue* json_object_get(const RogueJsonValue* object, const char* key)
{
    if (!object || object->type != JSON_OBJECT || !key)
    {
        return NULL;
    }

    for (size_t i = 0; i < object->data.object_value.count; i++)
    {
        if (strcmp(object->data.object_value.keys[i], key) == 0)
        {
            return object->data.object_value.values[i];
        }
    }

    return NULL;
}

/**
 * @brief Checks if a JSON object contains a specific key.
 * @param object Pointer to the JSON object value.
 * @param key The key to check for.
 * @return true if the key exists, false otherwise.
 * @details Convenience function that checks for key existence without retrieving the value.
 */
bool json_object_has_key(const RogueJsonValue* object, const char* key)
{
    return json_object_get(object, key) != NULL;
}

/* ===== Memory Management ===== */

/**
 * @brief Frees a JSON value and all its associated memory.
 * @param json Pointer to the JSON value to free.
 * @details Recursively frees all nested JSON values, strings, and arrays/objects.
 * Safe to call with NULL.
 */
void json_free(RogueJsonValue* json)
{
    if (!json)
        return;

    switch (json->type)
    {
    case JSON_STRING:
        free(json->data.string_value);
        break;

    case JSON_ARRAY:
        for (size_t i = 0; i < json->data.array_value.count; i++)
        {
            json_free(json->data.array_value.items[i]);
        }
        free(json->data.array_value.items);
        break;

    case JSON_OBJECT:
        for (size_t i = 0; i < json->data.object_value.count; i++)
        {
            free(json->data.object_value.keys[i]);
            json_free(json->data.object_value.values[i]);
        }
        free(json->data.object_value.keys);
        free(json->data.object_value.values);
        break;

    default:
        /* Primitive types don't need special cleanup */
        break;
    }

    free(json);
}

/* ===== Utility Functions ===== */

/**
 * @brief Converts a JSON type enum to its string representation.
 * @param type The JSON type to convert.
 * @return String representation of the type.
 * @details Returns human-readable names for each JSON type.
 */
const char* json_type_to_string(JsonType type)
{
    switch (type)
    {
    case JSON_NULL:
        return "null";
    case JSON_BOOLEAN:
        return "boolean";
    case JSON_INTEGER:
        return "integer";
    case JSON_NUMBER:
        return "number";
    case JSON_STRING:
        return "string";
    case JSON_ARRAY:
        return "array";
    case JSON_OBJECT:
        return "object";
    default:
        return "unknown";
    }
}
