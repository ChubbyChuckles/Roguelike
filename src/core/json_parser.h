#ifndef ROGUE_JSON_PARSER_H
#define ROGUE_JSON_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Simple JSON value types for schema validation testing */
typedef enum {
    JSON_NULL = 0,
    JSON_BOOLEAN = 1,
    JSON_INTEGER = 2,
    JSON_NUMBER = 3,
    JSON_STRING = 4,
    JSON_ARRAY = 5,
    JSON_OBJECT = 6
} JsonType;

typedef struct RogueJsonValue RogueJsonValue;

typedef struct {
    char** keys;
    RogueJsonValue** values;
    size_t count;
} JsonObject;

typedef struct {
    RogueJsonValue** items;
    size_t count;
} JsonArray;

struct RogueJsonValue {
    JsonType type;
    union {
        bool boolean_value;
        int64_t integer_value;
        double number_value;
        char* string_value;
        JsonArray array_value;
        JsonObject object_value;
    } data;
};

/* Function declarations for JSON value management */
RogueJsonValue* json_create_null(void);
RogueJsonValue* json_create_boolean(bool value);
RogueJsonValue* json_create_integer(int64_t value);
RogueJsonValue* json_create_number(double value);
RogueJsonValue* json_create_string(const char* value);
RogueJsonValue* json_create_array(void);
RogueJsonValue* json_create_object(void);

/* JSON manipulation functions */
bool json_array_add(RogueJsonValue* array, RogueJsonValue* item);
bool json_object_set(RogueJsonValue* object, const char* key, RogueJsonValue* value);
RogueJsonValue* json_object_get(const RogueJsonValue* object, const char* key);
bool json_object_has_key(const RogueJsonValue* object, const char* key);

/* Memory management */
void json_free(RogueJsonValue* json);

/* Utility functions */
const char* json_type_to_string(JsonType type);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_JSON_PARSER_H */
