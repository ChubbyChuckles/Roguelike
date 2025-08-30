/**
 * @file world_gen_biome_json.c
 * @brief JSON parsing and validation for biome registries.
 * @details This module provides functionality to load biome descriptors from JSON text,
 * validate biome balance, build transition matrices, and validate encounter tables.
 */

#include "world_gen_biome_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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
 * @brief Skips whitespace characters in the JSON string.
 * @param ps Pointer to the current position in the string.
 */
static void skip_ws(const char** ps)
{
    const char* s = *ps;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        s++;
    *ps = s;
}

/**
 * @brief Parses a JSON string value.
 * @param ps Pointer to the current position in the string.
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
 * @brief Parses a JSON number value.
 * @param ps Pointer to the current position in the string.
 * @param out Pointer to store the parsed double value.
 * @return 1 on success, 0 on failure.
 */
static int parse_number(const char** ps, double* out)
{
    char* e = NULL;
    double v = strtod(*ps, &e);
    if (e == *ps)
        return 0;
    *out = v;
    *ps = e;
    return 1;
}

/**
 * @brief Loads biome descriptors from JSON text into the registry.
 * @param reg Pointer to the biome registry.
 * @param json_text JSON text containing biome descriptors.
 * @param err Error buffer.
 * @param err_cap Capacity of the error buffer.
 * @return Number of biomes added on success, -1 on failure.
 */
int rogue_biome_registry_load_json_text(RogueBiomeRegistry* reg, const char* json_text, char* err,
                                        size_t err_cap)
{
    if (!reg || !json_text)
    {
        set_err(err, err_cap, "invalid args");
        return -1;
    }
    const char* s = json_text;
    skip_ws(&s);
    if (*s != '[')
    {
        set_err(err, err_cap, "expected array");
        return -1;
    }
    s++;
    int added = 0;
    while (1)
    {
        skip_ws(&s);
        if (*s == ']')
        {
            s++;
            break;
        }
        if (*s != '{')
        {
            set_err(err, err_cap, "expected object");
            return -1;
        }
        s++;
        RogueBiomeDescriptor d;
        memset(&d, 0, sizeof d);
        /* defaults similar to CFG parser */
        d.vegetation_density = 0.3f;
        d.decoration_density = 0.2f;
        d.ambient_color[0] = d.ambient_color[1] = d.ambient_color[2] = 70;
#ifdef _MSC_VER
        strncpy_s(d.music_track, sizeof d.music_track, "default", _TRUNCATE);
        strncpy_s(d.name, sizeof d.name, "unnamed", _TRUNCATE);
#else
        strncpy(d.music_track, "default", sizeof d.music_track - 1);
        strncpy(d.name, "unnamed", sizeof d.name - 1);
#endif
        while (1)
        {
            skip_ws(&s);
            if (*s == '}')
            {
                s++;
                break;
            }
            skip_ws(&s);
            if (*s != '"')
            {
                set_err(err, err_cap, "expected key");
                return -1;
            }
            char key[32];
            if (!parse_string(&s, key, sizeof key))
            {
                set_err(err, err_cap, "bad key");
                return -1;
            }
            skip_ws(&s);
            if (*s != ':')
            {
                set_err(err, err_cap, "expected colon");
                return -1;
            }
            s++;
            skip_ws(&s);
            if (strcmp(key, "name") == 0)
            {
                char tmp[64];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -1;
#ifdef _MSC_VER
                strncpy_s(d.name, sizeof d.name, tmp, _TRUNCATE);
#else
                strncpy(d.name, tmp, sizeof d.name - 1);
#endif
            }
            else if (strcmp(key, "music") == 0)
            {
                char tmp[64];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -1;
#ifdef _MSC_VER
                strncpy_s(d.music_track, sizeof d.music_track, tmp, _TRUNCATE);
#else
                strncpy(d.music_track, tmp, sizeof d.music_track - 1);
#endif
            }
            else if (strcmp(key, "vegetation_density") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                if (v < 0)
                    v = 0;
                if (v > 1)
                    v = 1;
                d.vegetation_density = (float) v;
            }
            else if (strcmp(key, "decoration_density") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                if (v < 0)
                    v = 0;
                if (v > 1)
                    v = 1;
                d.decoration_density = (float) v;
            }
            else if (strcmp(key, "ambient_color") == 0)
            {
                skip_ws(&s);
                if (*s != '[')
                    return -1;
                s++;
                double r = 70, g = 70, b = 70;
                if (!parse_number(&s, &r))
                    return -1;
                skip_ws(&s);
                if (*s == ',')
                {
                    s++;
                }
                if (!parse_number(&s, &g))
                    return -1;
                skip_ws(&s);
                if (*s == ',')
                {
                    s++;
                }
                if (!parse_number(&s, &b))
                    return -1;
                skip_ws(&s);
                if (*s == ']')
                    s++;
                else
                    return -1;
                if (r < 0)
                    r = 0;
                if (r > 255)
                    r = 255;
                if (g < 0)
                    g = 0;
                if (g > 255)
                    g = 255;
                if (b < 0)
                    b = 0;
                if (b > 255)
                    b = 255;
                d.ambient_color[0] = (unsigned char) r;
                d.ambient_color[1] = (unsigned char) g;
                d.ambient_color[2] = (unsigned char) b;
            }
            else if (strcmp(key, "allow_structures") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                d.allow_structures = (unsigned char) (v != 0);
            }
            else if (strcmp(key, "allow_weather") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                d.allow_weather = (unsigned char) (v != 0);
            }
            else if (strncmp(key, "tile_", 5) == 0)
            {
                /* numeric weight */
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                int t = -1;
                const char* suf = key + 5;
                /* map known tiles */
                if (strcmp(suf, "grass") == 0)
                    t = ROGUE_TILE_GRASS;
                else if (strcmp(suf, "forest") == 0)
                    t = ROGUE_TILE_FOREST;
                else if (strcmp(suf, "water") == 0)
                    t = ROGUE_TILE_WATER;
                else if (strcmp(suf, "mountain") == 0)
                    t = ROGUE_TILE_MOUNTAIN;
                else if (strcmp(suf, "swamp") == 0)
                    t = ROGUE_TILE_SWAMP;
                else if (strcmp(suf, "snow") == 0)
                    t = ROGUE_TILE_SNOW;
                else if (strcmp(suf, "river") == 0)
                    t = ROGUE_TILE_RIVER;
                if (t >= 0)
                    d.tile_weights[t] = (float) v;
            }
            /* consume optional comma */
            skip_ws(&s);
            if (*s == ',')
            {
                s++;
            }
        }
        /* post-process: ensure weights normalize & count */
        double sum = 0.0;
        int any = 0;
        for (int i = 0; i < ROGUE_TILE_MAX; i++)
        {
            if (d.tile_weights[i] > 0)
            {
                sum += d.tile_weights[i];
                any = 1;
            }
        }
        if (!any || sum <= 0.0)
        {
            set_err(err, err_cap, "biome missing tile weights");
            return -1;
        }
        d.tile_weight_count = 0;
        for (int i = 0; i < ROGUE_TILE_MAX; i++)
        {
            if (d.tile_weights[i] > 0)
            {
                d.tile_weights[i] /= (float) sum;
                d.tile_weight_count++;
            }
        }
        if (rogue_biome_registry_add(reg, &d) < 0)
        {
            set_err(err, err_cap, "registry add failed");
            return -1;
        }
        added++;
        skip_ws(&s);
        if (*s == ',')
        {
            s++;
            continue;
        }
        if (*s == ']')
        {
            s++;
            break;
        }
    }
    return added;
}

/**
 * @brief Validates the balance of biomes in the registry.
 * @param reg Pointer to the biome registry.
 * @param veg_min Minimum vegetation density.
 * @param veg_max Maximum vegetation density.
 * @param deco_min Minimum decoration density.
 * @param deco_max Maximum decoration density.
 * @param err Error buffer.
 * @param err_cap Capacity of the error buffer.
 * @return 1 if valid, 0 otherwise.
 */
int rogue_biome_registry_validate_balance(const RogueBiomeRegistry* reg, float veg_min,
                                          float veg_max, float deco_min, float deco_max, char* err,
                                          size_t err_cap)
{
    if (!reg || reg->count <= 0)
    {
        set_err(err, err_cap, "empty registry");
        return 0;
    }
    for (int i = 0; i < reg->count; i++)
    {
        const RogueBiomeDescriptor* d = &reg->biomes[i];
        if (d->tile_weight_count <= 0)
        {
            set_err(err, err_cap, "biome has no tiles");
            return 0;
        }
        if (d->vegetation_density < veg_min || d->vegetation_density > veg_max)
        {
            set_err(err, err_cap, "vegetation out of range");
            return 0;
        }
        if (d->decoration_density < deco_min || d->decoration_density > deco_max)
        {
            set_err(err, err_cap, "decoration out of range");
            return 0;
        }
    }
    (void) err;
    (void) err_cap;
    return 1;
}

/**
 * @brief Finds the index of a biome by name.
 * @param reg Pointer to the biome registry.
 * @param name Name of the biome.
 * @return Index of the biome, or -1 if not found.
 */
static int find_biome_index(const RogueBiomeRegistry* reg, const char* name)
{
    for (int i = 0; i < reg->count; i++)
        if (strncmp(reg->biomes[i].name, name, sizeof reg->biomes[i].name) == 0)
            return i;
    return -1;
}

/**
 * @brief Builds a transition matrix from JSON text.
 * @param reg Pointer to the biome registry.
 * @param json_text JSON text defining transitions.
 * @param out_matrix Output matrix buffer.
 * @param out_cap Capacity of the output matrix.
 * @param err Error buffer.
 * @param err_cap Capacity of the error buffer.
 * @return 1 on success, 0 on failure.
 */
int rogue_biome_build_transition_matrix(const RogueBiomeRegistry* reg, const char* json_text,
                                        unsigned char* out_matrix, int out_cap, char* err,
                                        size_t err_cap)
{
    if (!reg || !json_text || !out_matrix)
    {
        set_err(err, err_cap, "invalid args");
        return 0;
    }
    int need = reg->count * reg->count;
    if (out_cap < need)
    {
        set_err(err, err_cap, "matrix too small");
        return 0;
    }
    memset(out_matrix, 0, (size_t) need);
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
        char key[64];
        if (!parse_string(&s, key, sizeof key))
        {
            set_err(err, err_cap, "bad key");
            return 0;
        }
        int src = find_biome_index(reg, key);
        skip_ws(&s);
        if (*s != ':')
        {
            return 0;
        }
        s++;
        skip_ws(&s);
        if (*s != '[')
        {
            return 0;
        }
        s++;
        while (1)
        {
            skip_ws(&s);
            if (*s == ']')
            {
                s++;
                break;
            }
            char v[64];
            if (!parse_string(&s, v, sizeof v))
                return 0;
            int dst = find_biome_index(reg, v);
            if (src >= 0 && dst >= 0)
                out_matrix[src * reg->count + dst] = 1;
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

/**
 * @brief Validates encounter tables from JSON text.
 * @param reg Pointer to the biome registry.
 * @param json_text JSON text containing encounter tables.
 * @param err Error buffer.
 * @param err_cap Capacity of the error buffer.
 * @return 1 if valid, 0 otherwise.
 */
int rogue_biome_validate_encounter_tables(const RogueBiomeRegistry* reg, const char* json_text,
                                          char* err, size_t err_cap)
{
    if (!reg || !json_text)
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
    /* track seen biomes */
    unsigned char* seen = (unsigned char*) calloc((size_t) reg->count, 1);
    if (!seen)
    {
        set_err(err, err_cap, "oom");
        return 0;
    }
    int ok = 1;
    while (ok)
    {
        skip_ws(&s);
        if (*s == '}')
        {
            s++;
            break;
        }
        char key[64];
        if (!parse_string(&s, key, sizeof key))
        {
            ok = 0;
            break;
        }
        int idx = find_biome_index(reg, key);
        skip_ws(&s);
        if (*s != ':')
        {
            ok = 0;
            break;
        }
        s++;
        skip_ws(&s);
        if (*s != '[')
        {
            ok = 0;
            break;
        }
        s++;
        int count = 0;
        while (1)
        {
            skip_ws(&s);
            if (*s == ']')
            {
                s++;
                break;
            }
            char val[64];
            if (!parse_string(&s, val, sizeof val))
            {
                ok = 0;
                break;
            }
            count++;
            skip_ws(&s);
            if (*s == ',')
            {
                s++;
                continue;
            }
        }
        if (idx >= 0)
        {
            if (count <= 0)
            {
                ok = 0;
                break;
            }
            seen[idx] = 1;
        }
        skip_ws(&s);
        if (*s == ',')
        {
            s++;
            continue;
        }
    }
    if (ok)
    {
        for (int i = 0; i < reg->count; i++)
        {
            if (!seen[i])
            {
                ok = 0;
                break;
            }
        }
        if (!ok)
            set_err(err, err_cap, "missing biome encounter table");
    }
    free(seen);
    return ok;
}
