#include "world/world_gen_resource_json.h"
#include "world/world_gen.h"
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

/* Accepts keys: id (string), rarity (int), tool_tier (int), yield_min (int), yield_max (int),
 * biome_mask (number, optional), biomes (array of strings, optional; ORed into mask).
 */
int rogue_resource_defs_load_json_text(const char* json_text, char* err, size_t err_cap)
{
    if (!json_text)
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
        RogueResourceNodeDesc d;
        memset(&d, 0, sizeof d);
#ifdef _MSC_VER
        strncpy_s(d.id, sizeof d.id, "unnamed", _TRUNCATE);
#else
        strncpy(d.id, "unnamed", sizeof d.id - 1);
#endif
        d.yield_min = 1;
        d.yield_max = 1;
        d.tool_tier = 0;
        d.rarity = 0;
        d.biome_mask = 0u;
        while (1)
        {
            skip_ws(&s);
            if (*s == '}')
            {
                s++;
                break;
            }
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
            if (strcmp(key, "id") == 0)
            {
                char tmp[64];
                if (!parse_string(&s, tmp, sizeof tmp))
                    return -1;
#ifdef _MSC_VER
                strncpy_s(d.id, sizeof d.id, tmp, _TRUNCATE);
#else
                strncpy(d.id, tmp, sizeof d.id - 1);
#endif
            }
            else if (strcmp(key, "rarity") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                d.rarity = (int) v;
            }
            else if (strcmp(key, "tool_tier") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                d.tool_tier = (int) v;
            }
            else if (strcmp(key, "yield_min") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                d.yield_min = (int) v;
            }
            else if (strcmp(key, "yield_max") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                d.yield_max = (int) v;
            }
            else if (strcmp(key, "biome_mask") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return -1;
                if (v < 0)
                    v = 0;
                d.biome_mask = (unsigned int) v;
            }
            else if (strcmp(key, "biomes") == 0)
            {
                skip_ws(&s);
                if (*s != '[')
                    return -1;
                s++;
                while (1)
                {
                    skip_ws(&s);
                    if (*s == ']')
                    {
                        s++;
                        break;
                    }
                    char name[32];
                    if (!parse_string(&s, name, sizeof name))
                        return -1;
                    /* Map common biome names to mask bits */
                    if (strcmp(name, "Plains") == 0)
                        d.biome_mask |= (1u << ROGUE_BIOME_PLAINS);
                    else if (strcmp(name, "Forest") == 0)
                        d.biome_mask |= (1u << ROGUE_BIOME_FOREST_BIOME);
                    else if (strcmp(name, "Mountain") == 0)
                        d.biome_mask |= (1u << ROGUE_BIOME_MOUNTAIN_BIOME);
                    else if (strcmp(name, "Snow") == 0)
                        d.biome_mask |= (1u << ROGUE_BIOME_SNOW_BIOME);
                    else if (strcmp(name, "Swamp") == 0)
                        d.biome_mask |= (1u << ROGUE_BIOME_SWAMP_BIOME);
                    else if (strcmp(name, "Ocean") == 0)
                        d.biome_mask |= (1u << ROGUE_BIOME_OCEAN);
                    skip_ws(&s);
                    if (*s == ',')
                    {
                        s++;
                        continue;
                    }
                }
            }
            /* consume optional comma */
            skip_ws(&s);
            if (*s == ',')
                s++;
        }
        if (d.yield_min < 0 || d.yield_max < d.yield_min)
        {
            set_err(err, err_cap, "invalid yield range");
            return -1;
        }
        if (d.biome_mask == 0u)
        {
            /* default to all non-ocean biomes */
            d.biome_mask = (1u << ROGUE_BIOME_PLAINS) | (1u << ROGUE_BIOME_FOREST_BIOME) |
                           (1u << ROGUE_BIOME_MOUNTAIN_BIOME) | (1u << ROGUE_BIOME_SNOW_BIOME) |
                           (1u << ROGUE_BIOME_SWAMP_BIOME);
        }
        if (rogue_resource_register(&d) < 0)
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
