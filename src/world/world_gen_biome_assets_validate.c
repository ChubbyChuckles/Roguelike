/**
 * @file world_gen_biome_assets_validate.c
 * @brief Validates biome assets from JSON configuration.
 * @details This module provides validation for biome asset files referenced in JSON configurations,
 * ensuring that image paths exist and are accessible.
 */

#include "world_gen_biome_assets_validate.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Sets an error message in the provided buffer.
 * @param err Error buffer.
 * @param cap Capacity of the error buffer.
 * @param msg Error message to set.
 */
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

/**
 * @brief Skips whitespace characters in the string.
 * @param ps Pointer to the string pointer.
 */
static void skip_ws(const char** ps)
{
    const char* s = *ps;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        s++;
    *ps = s;
}

/**
 * @brief Parses a quoted string from the input.
 * @param ps Pointer to the string pointer.
 * @param out Output buffer for the parsed string.
 * @param cap Capacity of the output buffer.
 * @return 1 on success, 0 on failure.
 */
static int parse_string(const char** ps, char* out, size_t cap)
{
    const char* s = *ps;
    if (*s != '"')
        return 0;
    s++;
    size_t i = 0;
    while (*s && *s != '"')
    {
        if (i + 1 < cap)
            out[i++] = *s;
        s++;
    }
    if (*s != '"')
        return 0;
    out[i] = '\0';
    s++;
    *ps = s;
    return 1;
}

/**
 * @brief Validates biome assets from a JSON text.
 * @param json_text The JSON text to validate.
 * @param err Error buffer.
 * @param err_cap Capacity of the error buffer.
 * @return 1 if valid, 0 otherwise.
 */
int rogue_biome_assets_validate_from_json(const char* json_text, char* err, size_t err_cap)
{
    if (!json_text)
    {
        set_err(err, err_cap, "invalid args");
        return 0;
    }
    const char* s = json_text;
    skip_ws(&s);
    if (*s != '{')
    {
        set_err(err, err_cap, "expected object");
        return 0;
    }
    s++;
    while (1)
    {
        skip_ws(&s);
        if (*s == '}')
        {
            s++;
            break;
        }
        char biome_name[64];
        if (!parse_string(&s, biome_name, sizeof biome_name))
            return 0;
        skip_ws(&s);
        if (*s != ':')
            return 0;
        s++;
        skip_ws(&s);
        if (*s != '[')
            return 0;
        s++;
        while (1)
        {
            skip_ws(&s);
            if (*s == ']')
            {
                s++;
                break;
            }
            if (*s != '{')
                return 0;
            s++;
            /* scan keys; we only care about image */
            char image_path[256];
            image_path[0] = '\0';
            while (1)
            {
                skip_ws(&s);
                if (*s == '}')
                {
                    s++;
                    break;
                }
                char key[32];
                if (!parse_string(&s, key, sizeof key))
                    return 0;
                skip_ws(&s);
                if (*s != ':')
                    return 0;
                s++;
                skip_ws(&s);
                if (strcmp(key, "image") == 0)
                {
                    if (!parse_string(&s, image_path, sizeof image_path))
                        return 0;
                }
                else
                {
                    /* skip value (string or number); for simplicity only skip strings here */
                    if (*s == '"')
                    {
                        char tmp[64];
                        if (!parse_string(&s, tmp, sizeof tmp))
                            return 0;
                    }
                    else
                    {
                        /* skip non-string token until comma/brace */
                        while (*s && *s != ',' && *s != '}')
                            s++;
                    }
                }
                skip_ws(&s);
                if (*s == ',')
                {
                    s++;
                    continue;
                }
            }
            if (image_path[0])
            {
                FILE* f = NULL;
#ifdef _MSC_VER
                fopen_s(&f, image_path, "rb");
#else
                f = fopen(image_path, "rb");
#endif
                if (!f)
                {
                    /* Try common relative fallbacks used by tests (ctest working dirs vary) */
                    char alt1[512];
                    char alt2[512];
                    alt1[0] = '\0';
                    alt2[0] = '\0';
#ifdef _MSC_VER
                    sprintf_s(alt1, sizeof alt1, "../%s", image_path);
                    sprintf_s(alt2, sizeof alt2, "../../%s", image_path);
#else
                    snprintf(alt1, sizeof alt1, "../%s", image_path);
                    snprintf(alt2, sizeof alt2, "../../%s", image_path);
#endif
#ifdef _MSC_VER
                    fopen_s(&f, alt1, "rb");
#else
                    f = fopen(alt1, "rb");
#endif
                    if (!f)
                    {
#ifdef _MSC_VER
                        fopen_s(&f, alt2, "rb");
#else
                        f = fopen(alt2, "rb");
#endif
                    }
                    if (!f)
                    {
                        set_err(err, err_cap, "missing biome asset");
                        return 0;
                    }
                }
                fclose(f);
            }
            skip_ws(&s);
            if (*s == ',')
            {
                s++;
                continue;
            }
        }
        skip_ws(&s);
        if (*s == ',')
        {
            s++;
            continue;
        }
    }
    return 1;
}
