#include "core/vegetation/vegetation_json.h"
#include "core/vegetation/vegetation_internal.h" /* for rogue__vegetation_register_def */
#include <stdlib.h>
#include <string.h>

extern int rogue_vegetation_count(void); /* for local checks */

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
static void skip_ws(const char** ps)
{
    const char* s = *ps;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        s++;
    *ps = s;
}
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
static int parse_int(const char** ps, int* out)
{
    char* e = NULL;
    long v = strtol(*ps, &e, 10);
    if (e == *ps)
        return 0;
    *out = (int) v;
    *ps = e;
    return 1;
}

static int parse_array_of_objects(const char* json_text, int want_tree, char* err, size_t err_cap)
{
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
        RogueVegetationDef d;
        memset(&d, 0, sizeof d);
        d.is_tree = (unsigned char) want_tree;
        d.rarity = 1;
        d.canopy_radius = want_tree ? 2 : 0;
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
                return -1;
            skip_ws(&s);
            if (*s != ':')
                return -1;
            s++;
            skip_ws(&s);
            if (strcmp(key, "id") == 0)
            {
                char tmp[64];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -1;
                strncpy_s(d.id, sizeof d.id, tmp, _TRUNCATE);
            }
            else if (strcmp(key, "image") == 0 || strcmp(key, "sprite") == 0)
            {
                char tmp[128];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -1;
                strncpy_s(d.image, sizeof d.image, tmp, _TRUNCATE);
            }
            else if (strcmp(key, "tx") == 0)
            {
                int v;
                if (!parse_int(&s, &v))
                    return -1;
                d.tile_x = (unsigned short) v;
                d.tile_x2 = d.tile_x;
            }
            else if (strcmp(key, "ty") == 0)
            {
                int v;
                if (!parse_int(&s, &v))
                    return -1;
                d.tile_y = (unsigned short) v;
                d.tile_y2 = d.tile_y;
            }
            else if (strcmp(key, "tx2") == 0)
            {
                int v;
                if (!parse_int(&s, &v))
                    return -1;
                d.tile_x2 = (unsigned short) v;
            }
            else if (strcmp(key, "ty2") == 0)
            {
                int v;
                if (!parse_int(&s, &v))
                    return -1;
                d.tile_y2 = (unsigned short) v;
            }
            else if (strcmp(key, "rarity") == 0)
            {
                int v;
                if (!parse_int(&s, &v))
                    return -1;
                d.rarity = (unsigned short) (v <= 0 ? 1 : v);
            }
            else if (strcmp(key, "canopy_radius") == 0)
            {
                int v;
                if (!parse_int(&s, &v))
                    return -1;
                d.canopy_radius = (unsigned char) (v <= 0 ? 1 : v);
            }
            /* consume optional comma */
            skip_ws(&s);
            if (*s == ',')
            {
                s++;
            }
        }
        /* Insert into defs by reusing existing loader path: emulate one CSV line */
        /* We don't have a direct add function; use internal arrays by calling generation helpers.
         */
        /* Hack: reuse parse_line by building a CSV and passing through rogue_vegetation_load_defs
           would be heavy. Instead, expose minimal internal registration via this file: */
        if (!rogue__vegetation_register_def(&d))
        {
            set_err(err, err_cap, "vegetation register failed");
            return -1;
        }
        added++;
        skip_ws(&s);
        if (*s == ',')
        {
            s++;
            continue;
        }
    }
    (void) err;
    (void) err_cap;
    return added;
}

int rogue_vegetation_load_plants_json_text(const char* json_text, char* err, size_t err_cap)
{
    return parse_array_of_objects(json_text, 0, err, err_cap);
}

int rogue_vegetation_load_trees_json_text(const char* json_text, char* err, size_t err_cap)
{
    return parse_array_of_objects(json_text, 1, err, err_cap);
}
