/**
 * @file json_envelope.c
 * @brief JSON Envelope Creation and Parsing Module.
 *
 * This module provides functionality for creating and parsing JSON envelopes used in the Rogue
 * game for serializing game data such as schemas and versions. It ensures data is wrapped in a
 * standard format with schema reference, version, and entries payload. Supports error reporting
 * via optional error buffers. Includes helper functions for string duplication, whitespace
 * skipping, key finding in JSON, and basic parsing of strings and unsigned integers from JSON
 * values.
 *
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 */

#define _CRT_SECURE_NO_WARNINGS
#include "json_envelope.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Sets an error message in the provided buffer.
 *
 * Safely copies the error message into the buffer, truncating if necessary, or clears it if no
 * message.
 *
 * @param err Buffer to store the error message (may be NULL).
 * @param cap Capacity of the error buffer (including null terminator).
 * @param msg Error message to set (may be NULL to clear).
 */
static void set_err(char* err, int cap, const char* msg)
{
    if (!err || cap <= 0)
        return;
    if (!msg)
    {
        err[0] = '\0';
        return;
    }
    snprintf(err, (size_t) cap, "%s", msg);
}

/**
 * @brief Duplicates a string using malloc.
 *
 * Allocates memory for a copy of the input string and copies it. Returns NULL on failure or null
 * input.
 *
 * @param s Input string to duplicate (may be NULL).
 * @return Allocated copy of the string, or NULL on error.
 */
static char* str_dup(const char* s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char* r = (char*) malloc(n);
    if (r)
        memcpy(r, s, n);
    return r;
}

/**
 * @brief Creates a JSON envelope string.
 *
 * Composes a JSON envelope by injecting the provided schema, version, and entries JSON (assumed
 * valid by caller) into a formatted string. Allocates memory for the result. Caller must free the
 * output using free(). Supports error reporting.
 *
 * @param schema Schema URI or identifier string (required, non-null).
 * @param version Version number for the envelope (unsigned 32-bit).
 * @param entries_json Valid JSON string for the entries payload (required, non-null).
 * @param[out] out_json Pointer to receive the allocated JSON string (required, will be set on
 * success).
 * @param err Buffer for error message (optional).
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, 1 for invalid args, 2 for out-of-memory.
 */
int json_envelope_create(const char* schema, uint32_t version, const char* entries_json,
                         char** out_json, char* err, int err_cap)
{
    if (!schema || !entries_json || !out_json)
    {
        set_err(err, err_cap, "invalid args");
        return 1;
    }

    /* Compose JSON text with entries_json injected verbatim (caller must ensure it's valid JSON).
     */
    const char* fmt = "{\n  \"$schema\": \"%s\",\n  \"version\": %u,\n  \"entries\": %s\n}";
    size_t cap = strlen(fmt) + strlen(schema) + 32 + strlen(entries_json) + 1;
    char* buf = (char*) malloc(cap);
    if (!buf)
    {
        set_err(err, err_cap, "oom");
        return 2;
    }
    snprintf(buf, cap, fmt, schema, version, entries_json);
    *out_json = buf;
    return 0;
}

/**
 * @brief Skips whitespace characters in a string.
 *
 * Advances the pointer past consecutive whitespace characters.
 *
 * @param p Pointer to the string position to start skipping from.
 * @return Pointer to the first non-whitespace character, or end of string.
 */
static const char* skip_ws(const char* p)
{
    while (p && *p && isspace((unsigned char) *p))
        ++p;
    return p;
}

/**
 * @brief Finds a key in JSON text and returns pointer to its value after colon.
 *
 * Simple string-based search for a quoted key, then skips to after colon and whitespace.
 * Assumes well-formed JSON; not a full parser.
 *
 * @param json The JSON string to search in.
 * @param key The key name to find (without quotes).
 * @return Pointer to value start, or NULL if key not found.
 */
static const char* find_key(const char* json, const char* key)
{
    char pat[128];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* s = strstr(json, pat);
    if (!s)
        return NULL;
    s = strchr(s + strlen(pat), ':');
    if (!s)
        return NULL;
    return skip_ws(s + 1);
}

/**
 * @brief Parses a JSON string value starting at the opening quote.
 *
 * Extracts the string content, handling basic escapes, allocates memory for it.
 *
 * @param p Pointer to the opening quote of the string value.
 * @param[out] out Pointer to receive the allocated string (caller must free).
 * @return 0 on success, 1 for invalid start, 2 for unterminated, 3 for out-of-memory.
 */
static int parse_string_value(const char* p, char** out)
{
    if (!p || *p != '"')
        return 1;
    ++p; /* after opening quote */
    const char* start = p;
    while (*p && *p != '"')
    {
        if (*p == '\\' && p[1] != '\0')
            ++p; /* skip escaped char */
        ++p;
    }
    if (*p != '"')
        return 2;
    size_t n = (size_t) (p - start);
    char* s = (char*) malloc(n + 1);
    if (!s)
        return 3;
    memcpy(s, start, n);
    s[n] = '\0';
    *out = s;
    return 0;
}

/**
 * @brief Parses an unsigned integer value from JSON text.
 *
 * Skips whitespace, parses digits, checks for overflow beyond uint32_t.
 *
 * @param p Pointer to the start of the number value.
 * @param[out] out Pointer to receive the parsed uint32_t value.
 * @return 0 on success, 1 for null pointer, 2 for non-digit start, 3 for overflow.
 */
static int parse_uint_value(const char* p, uint32_t* out)
{
    if (!p)
        return 1;
    unsigned long long v = 0ULL;
    while (*p && isspace((unsigned char) *p))
        ++p;
    if (!isdigit((unsigned char) *p))
        return 2;
    while (*p && isdigit((unsigned char) *p))
    {
        v = v * 10ULL + (unsigned) (*p - '0');
        ++p;
    }
    if (v > 0xFFFFFFFFULL)
        return 3;
    *out = (uint32_t) v;
    return 0;
}

/**
 * @brief Parses a JSON envelope into a RogueJsonEnvelope structure.
 *
 * Extracts $schema (string), version (uint32_t), and entries (object or array as raw JSON
 * substring). Allocates memory for schema and entries; caller must free using json_envelope_free().
 * Supports error reporting. Basic parsing without full JSON validation.
 *
 * @param json_text Input JSON envelope string (required, non-null).
 * @param[out] out_env Pointer to receive the parsed envelope structure (required, will be zeroed
 * and set on success).
 * @param err Buffer for error message (optional).
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, 1 for invalid args, 2 for missing keys, 3 for invalid schema, 4 for invalid
 * version, 5 for invalid entries type, 6 for out-of-memory, 7 for unterminated entries.
 */
int json_envelope_parse(const char* json_text, RogueJsonEnvelope* out_env, char* err, int err_cap)
{
    if (!json_text || !out_env)
    {
        set_err(err, err_cap, "invalid args");
        return 1;
    }
    memset(out_env, 0, sizeof(*out_env));

    const char* p_schema = find_key(json_text, "$schema");
    const char* p_version = find_key(json_text, "version");
    const char* p_entries = find_key(json_text, "entries");
    if (!p_schema || !p_version || !p_entries)
    {
        set_err(err, err_cap, "missing required keys");
        return 2;
    }

    char* schema = NULL;
    if (parse_string_value(p_schema, &schema) != 0)
    {
        set_err(err, err_cap, "invalid $schema string");
        return 3;
    }

    uint32_t version = 0;
    if (parse_uint_value(p_version, &version) != 0)
    {
        free(schema);
        set_err(err, err_cap, "invalid version number");
        return 4;
    }

    /* entries can be object or array; capture the JSON text slice for the value */
    const char* e = p_entries;
    e = skip_ws(e);
    if (*e != '{' && *e != '[')
    {
        free(schema);
        set_err(err, err_cap, "entries must be object or array");
        return 5;
    }
    char open = *e;
    char close = (open == '{') ? '}' : ']';
    int depth = 0;
    const char* start = e;
    const char* cur = e;
    int in_string = 0;
    while (*cur)
    {
        char c = *cur;
        if (in_string)
        {
            if (c == '"' && cur[-1] != '\\')
                in_string = 0;
            ++cur;
            continue;
        }
        if (c == '"')
        {
            in_string = 1;
            ++cur;
            continue;
        }
        if (c == open)
            depth++;
        else if (c == close)
        {
            depth--;
            if (depth == 0)
            {
                size_t n = (size_t) ((cur - start) + 1);
                char* entries = (char*) malloc(n + 1);
                if (!entries)
                {
                    free(schema);
                    set_err(err, err_cap, "oom");
                    return 6;
                }
                memcpy(entries, start, n);
                entries[n] = '\0';
                out_env->schema = schema;
                out_env->version = version;
                out_env->entries = entries;
                return 0;
            }
        }
        ++cur;
    }

    free(schema);
    set_err(err, err_cap, "unterminated entries value");
    return 7;
}

/**
 * @brief Frees the allocated fields in a RogueJsonEnvelope structure.
 *
 * Releases memory for schema and entries strings, resets the structure to zero.
 *
 * @param env Pointer to the envelope structure to free (may be NULL).
 */
void json_envelope_free(RogueJsonEnvelope* env)
{
    if (!env)
        return;
    free(env->schema);
    free(env->entries);
    env->schema = NULL;
    env->entries = NULL;
    env->version = 0;
}
