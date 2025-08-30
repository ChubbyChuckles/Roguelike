/**
 * @file kv_schema.c
 * @brief Key-value file schema validation system.
 * @details This module provides validation for key-value configuration files against
 * predefined schemas, supporting type checking, required fields, and error reporting.
 */

/* kv_schema.c - Phase M3.2 schema validation */
#include "kv_schema.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Finds a field definition by key in the schema.
 * @param s Pointer to the schema to search.
 * @param key The key to find.
 * @return Pointer to the field definition, or NULL if not found.
 * @details Performs linear search through the schema's field definitions.
 */
static const RogueKVFieldDef* find_field(const RogueKVSchema* s, const char* key)
{
    for (int i = 0; i < s->field_count; i++)
        if (strcmp(s->fields[i].key, key) == 0)
            return &s->fields[i];
    return NULL;
}

/**
 * @brief Validates a key-value file against a schema.
 * @param file Pointer to the parsed key-value file.
 * @param schema Pointer to the schema to validate against.
 * @param out_values Array to store parsed field values.
 * @param max_values Maximum number of values that can be stored in out_values.
 * @param err_buf Buffer to store error messages (can be NULL).
 * @param err_buf_sz Size of the error buffer.
 * @return Number of validation errors encountered.
 * @details Parses and validates each entry in the file, checking types, required fields,
 * and collecting values. Errors are accumulated in err_buf if provided.
 */
int rogue_kv_validate(const RogueKVFile* file, const RogueKVSchema* schema,
                      RogueKVFieldValue* out_values, int max_values, char* err_buf, int err_buf_sz)
{
    if (err_buf && err_buf_sz > 0)
        err_buf[0] = '\0';
    if (!file || !schema)
        return -1;
    int cursor = 0;
    RogueKVEntry e;
    RogueKVError perr;
    int errors = 0;
    int value_count = 0;
    (void) perr;
    while (rogue_kv_next(file, &cursor, &e, &perr))
    {
        const RogueKVFieldDef* def = find_field(schema, e.key);
        if (!def)
        {
            if (err_buf && (int) strlen(err_buf) < err_buf_sz - 32)
            {
                char tmp[64];
                snprintf(tmp, sizeof tmp, "Unknown key '%s' line %d; ", e.key, e.line);
                size_t len = strlen(err_buf);
                if (len + strlen(tmp) < (size_t) err_buf_sz)
                    snprintf(err_buf + len, err_buf_sz - len, "%s", tmp);
            }
            errors++;
            continue;
        }
        if (value_count < max_values)
        {
            RogueKVFieldValue* v = &out_values[value_count++];
            v->def = def;
            v->present = 1;
            v->i = 0;
            v->f = 0.0;
            v->s = NULL;
            char* end = NULL;
            if (def->type == ROGUE_KV_INT)
            {
                long val = strtol(e.value, &end, 10);
                if (end == e.value)
                {
                    errors++;
                    if (err_buf)
                    {
                        size_t len = strlen(err_buf);
                        if (len + 8 < (size_t) err_buf_sz)
                            snprintf(err_buf + len, err_buf_sz - len, "%s", "bad int ");
                    }
                }
                v->i = val;
            }
            else if (def->type == ROGUE_KV_FLOAT)
            {
                double d = strtod(e.value, &end);
                if (end == e.value)
                {
                    errors++;
                    if (err_buf)
                    {
                        size_t len = strlen(err_buf);
                        if (len + 11 < (size_t) err_buf_sz)
                            snprintf(err_buf + len, err_buf_sz - len, "%s", "bad float ");
                    }
                }
                v->f = d;
            }
            else
            {
                v->s = e.value;
            }
        }
    }
    /* required check */
    for (int i = 0; i < schema->field_count; i++)
        if (schema->fields[i].required)
        {
            int found = 0;
            for (int j = 0; j < value_count; j++)
                if (out_values[j].def == &schema->fields[i])
                {
                    found = 1;
                    break;
                }
            if (!found)
            {
                errors++;
                if (err_buf)
                {
                    char tmp[64];
                    snprintf(tmp, sizeof tmp, "missing %s; ", schema->fields[i].key);
                    size_t len = strlen(err_buf);
                    if (len + strlen(tmp) < (size_t) err_buf_sz)
                        snprintf(err_buf + len, err_buf_sz - len, "%s", tmp);
                }
            }
        }
    return errors;
}
