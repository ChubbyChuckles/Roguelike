#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "tag_registry.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_err(char* err, size_t cap, const char* msg)
{
    if (err && cap)
    {
#ifdef _MSC_VER
        strncpy_s(err, cap, msg, _TRUNCATE);
#else
        strncpy(err, msg, cap - 1);
        err[cap - 1] = '\0';
#endif
    }
}

static const char* find_key(const char* s, const char* key)
{
    char pat[64];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char* p = strstr(s, pat);
    if (!p)
        return NULL;
    p = strchr(p, ':');
    if (!p)
        return NULL;
    return p + 1;
}

static int parse_string(const char** ps, char* out, size_t cap)
{
    const char* s = *ps;
    while (*s && *s != '"')
        ++s;
    if (*s != '"')
        return 0;
    ++s;
    size_t i = 0;
    while (*s && *s != '"')
    {
        if (i + 1 < cap)
            out[i++] = *s;
        ++s;
    }
    if (*s != '"')
        return 0;
    out[i] = '\0';
    *ps = s + 1;
    return 1;
}

static int is_valid_tag(const char* t)
{
    if (!t || !*t)
        return 0;
    for (const char* s = t; *s; ++s)
    {
        if (!(isalnum((unsigned char) *s) || *s == '_' || *s == '-'))
            return 0;
    }
    return 1;
}

static int contains(char** arr, int n, const char* s)
{
    for (int i = 0; i < n; ++i)
        if (strcmp(arr[i], s) == 0)
            return 1;
    return 0;
}

static int parse_tag_array(const char* s, const char* key, char** out, int max, int* out_n,
                           char* err, size_t err_cap)
{
    const char* p = find_key(s, key);
    if (!p)
    {
        *out_n = 0; /* allow missing => empty list */
        return 0;
    }
    while (*p && *p != '[')
        ++p;
    if (*p != '[')
    {
        set_err(err, err_cap, "expected array");
        return -1;
    }
    ++p;
    int n = 0;
    while (*p && *p != ']' && n < max)
    {
        while (*p && *p != '"' && *p != ']')
            ++p;
        if (*p == ']')
            break;
        char buf[64];
        if (!parse_string(&p, buf, sizeof buf))
        {
            set_err(err, err_cap, "bad string in tag array");
            return -1;
        }
        if (!is_valid_tag(buf))
        {
            set_err(err, err_cap, "invalid characters in tag");
            return -1;
        }
        if (contains(out, n, buf))
        {
            set_err(err, err_cap, "duplicate tag in category");
            return -1;
        }
        out[n] = _strdup(buf);
        if (!out[n])
        {
            set_err(err, err_cap, "oom");
            return -1;
        }
        ++n;
    }
    *out_n = n;
    return 0;
}

int rogue_tag_registry_validate_text(const char* json_text, char* err, size_t err_cap)
{
    if (!json_text)
    {
        set_err(err, err_cap, "null input");
        return -1;
    }
    /* version field optional, if present must be integer-like */
    const char* v = find_key(json_text, "version");
    if (v)
    {
        while (*v == ' ' || *v == '\t')
            ++v;
        if (!(*v >= '0' && *v <= '9'))
        {
            set_err(err, err_cap, "version must be numeric");
            return -1;
        }
    }
    char* skills[128] = {0};
    char* equipment[128] = {0};
    char* dungeon[128] = {0};
    int ns = 0, ne = 0, nd = 0;
    if (parse_tag_array(json_text, "skills", skills, 128, &ns, err, err_cap) < 0)
        goto fail;
    if (parse_tag_array(json_text, "equipment", equipment, 128, &ne, err, err_cap) < 0)
        goto fail;
    if (parse_tag_array(json_text, "dungeon", dungeon, 128, &nd, err, err_cap) < 0)
        goto fail;
    /* Cross-category duplicates allowed by design, only intra-category uniqueness enforced. */
    for (int i = 0; i < ns; ++i)
        free(skills[i]);
    for (int i = 0; i < ne; ++i)
        free(equipment[i]);
    for (int i = 0; i < nd; ++i)
        free(dungeon[i]);
    return 0;
fail:
    for (int i = 0; i < ns; ++i)
        free(skills[i]);
    for (int i = 0; i < ne; ++i)
        free(equipment[i]);
    for (int i = 0; i < nd; ++i)
        free(dungeon[i]);
    return -1;
}

int rogue_tag_registry_validate_file(const char* path, char* err, size_t err_cap)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        set_err(err, err_cap, "open failed");
        return -1;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        set_err(err, err_cap, "oom");
        return -1;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[rd] = '\0';
    int rc = rogue_tag_registry_validate_text(buf, err, err_cap);
    free(buf);
    return rc;
}
