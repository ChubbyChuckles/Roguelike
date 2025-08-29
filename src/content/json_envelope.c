#define _CRT_SECURE_NO_WARNINGS
#include "json_envelope.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static const char* skip_ws(const char* p)
{
    while (p && *p && isspace((unsigned char) *p))
        ++p;
    return p;
}

/* Very small helper to find a string key: returns pointer to value start after colon. */
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
