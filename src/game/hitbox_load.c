#include "game/hitbox_load.h"
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#include <strings.h>
#endif
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* skip_ws(const char* s)
{
    while (*s && (unsigned char) *s <= 32)
        ++s;
    return s;
}
static int match(const char** ps, const char* tok)
{
    const char* s = *ps;
    s = skip_ws(s);
    size_t n = strlen(tok);
    if (strncmp(s, tok, n) == 0)
    {
        *ps = s + n;
        return 1;
    }
    return 0;
}
static int parse_string(const char** ps, char* buf, int buf_sz)
{
    const char* s = *ps;
    s = skip_ws(s);
    if (*s != '"')
        return 0;
    ++s;
    int i = 0;
    while (*s && *s != '"')
    {
        if (i + 1 < buf_sz)
            buf[i++] = *s;
        s++;
    }
    if (*s != '"')
        return 0;
    buf[i] = '\0';
    s++;
    *ps = s;
    return 1;
}
static int parse_number(const char** ps, double* out)
{
    const char* s = *ps;
    s = skip_ws(s);
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s)
        return 0;
    *out = v;
    *ps = end;
    return 1;
}

static int expect(const char** ps, char c)
{
    const char* s = *ps;
    s = skip_ws(s);
    if (*s == c)
    {
        *ps = s + 1;
        return 1;
    }
    return 0;
}

int rogue_hitbox_load_sequence_from_memory(const char* json, RogueHitbox* out, int max_out,
                                           int* out_count)
{
    if (out_count)
        *out_count = 0;
    if (!json || !out || max_out <= 0)
        return 0;
    const char* s = json;
    if (!expect(&s, '['))
        return 0;
    int count = 0;
    while (1)
    {
        s = skip_ws(s);
        if (*s == ']')
        {
            s++;
            break;
        }
        if (!expect(&s, '{'))
            return 0;
        char key[32];
        char type_name[32] = {0};
        RogueHitbox temp;
        memset(&temp, 0, sizeof temp);
        int have_type = 0;
        int done_obj = 0;
        while (!done_obj)
        {
            s = skip_ws(s);
            if (*s == '}')
            {
                s++;
                break;
            }
            if (!parse_string(&s, key, sizeof key))
                return 0;
            if (!expect(&s, ':'))
                return 0;
            if (strcmp(key, "type") == 0)
            {
                if (!parse_string(&s, type_name, sizeof type_name))
                    return 0;
                have_type = 1;
            }
            else if (strcmp(key, "ax") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.capsule.ax = (float) v;
            }
            else if (strcmp(key, "ay") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.capsule.ay = (float) v;
            }
            else if (strcmp(key, "bx") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.capsule.bx = (float) v;
            }
            else if (strcmp(key, "by") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.capsule.by = (float) v;
            }
            else if (strcmp(key, "r") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.capsule.radius = (float) v;
            }
            else if (strcmp(key, "ox") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.arc.ox = (float) v;
            }
            else if (strcmp(key, "oy") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.arc.oy = (float) v;
            }
            else if (strcmp(key, "radius") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.arc.radius = (float) v;
            }
            else if (strcmp(key, "a0") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.arc.angle_start = (float) v;
            }
            else if (strcmp(key, "a1") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.arc.angle_end = (float) v;
            }
            else if (strcmp(key, "inner_radius") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.arc.inner_radius = (float) v;
            }
            else if (strcmp(key, "radius_chain") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.chain.radius = (float) v;
            }
            else if (strcmp(key, "radius_chain_alias") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.chain.radius = (float) v;
            }
            else if (strcmp(key, "radius") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.chain.radius = (float) v;
            }
            else if (strcmp(key, "points") == 0)
            {
                if (!expect(&s, '['))
                    return 0;
                temp.u.chain.count = 0;
                while (1)
                {
                    s = skip_ws(s);
                    if (*s == ']')
                    {
                        s++;
                        break;
                    }
                    if (!expect(&s, '['))
                        return 0;
                    double vx, vy;
                    if (!parse_number(&s, &vx))
                        return 0;
                    if (!expect(&s, ','))
                        return 0;
                    if (!parse_number(&s, &vy))
                        return 0;
                    if (!expect(&s, ']'))
                        return 0;
                    if (temp.u.chain.count < ROGUE_HITBOX_CHAIN_MAX_POINTS)
                    {
                        temp.u.chain.px[temp.u.chain.count] = (float) vx;
                        temp.u.chain.py[temp.u.chain.count] = (float) vy;
                        temp.u.chain.count++;
                    }
                    s = skip_ws(s);
                    if (*s == ',')
                    {
                        s++;
                        continue;
                    }
                }
            }
            else if (strcmp(key, "count") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.proj.projectile_count = (int) v;
            }
            else if (strcmp(key, "speed") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.proj.base_speed = (float) v;
            }
            else if (strcmp(key, "spread") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.proj.spread_radians = (float) v;
            }
            else if (strcmp(key, "center") == 0)
            {
                double v;
                if (!parse_number(&s, &v))
                    return 0;
                temp.u.proj.angle_center = (float) v;
            }
            else
            { /* skip unknown */
                double v;
                if (parse_number(&s, &v))
                {
                }
                else
                {
                    char tmp[64];
                    if (parse_string(&s, tmp, sizeof tmp))
                    {
                    }
                    else
                    {
                        if (*s == '{' || *s == '[')
                        {
                            int depth = 0;
                            char open = *s;
                            char close = (open == '{' ? '}' : ']');
                            do
                            {
                                if (*s == open)
                                    depth++;
                                else if (*s == close)
                                    depth--;
                                s++;
                            } while (*s && depth > 0);
                        }
                        else
                        {
                            s++;
                        }
                    }
                }
            }
            s = skip_ws(s);
            if (*s == ',')
            {
                s++;
                continue;
            }
        }
        if (!have_type)
            return 0; /* assign type & finalize */
        if (strcmp(type_name, "capsule") == 0)
        {
            temp.type = ROGUE_HITBOX_CAPSULE;
        }
        else if (strcmp(type_name, "arc") == 0)
        {
            temp.type = ROGUE_HITBOX_ARC;
        }
        else if (strcmp(type_name, "chain") == 0)
        {
            temp.type = ROGUE_HITBOX_CHAIN;
        }
        else if (strcmp(type_name, "projectile_spawn") == 0)
        {
            temp.type = ROGUE_HITBOX_PROJECTILE_SPAWN;
        }
        else
        {
            temp.type = 0;
        }
        if (temp.type && count < max_out)
        {
            out[count++] = temp;
        }
        s = skip_ws(s);
        if (*s == ',')
        {
            s++;
            continue;
        }
    }
    if (out_count)
        *out_count = count;
    return 1;
}

int rogue_hitbox_load_sequence_file(const char* path, RogueHitbox* out, int max_out, int* out_count)
{
    FILE* f = NULL;
#ifdef _MSC_VER
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return 0;
    }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return 0;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[rd] = '\0';
    int ok = rogue_hitbox_load_sequence_from_memory(buf, out, max_out, out_count);
    free(buf);
    return ok;
}

static void hitbox_bounds(const RogueHitbox* h, float* minx, float* miny, float* maxx, float* maxy)
{
    *minx = *miny = 1e9f;
    *maxx = *maxy = -1e9f;
    if (!h)
        return;
    switch (h->type)
    {
    case ROGUE_HITBOX_CAPSULE:
    {
        float r = h->u.capsule.radius;
        float ax = h->u.capsule.ax, ay = h->u.capsule.ay, bx = h->u.capsule.bx,
              by = h->u.capsule.by;
        *minx = (ax < bx ? ax : bx) - r;
        *maxx = (ax > bx ? ax : bx) + r;
        *miny = (ay < by ? ay : by) - r;
        *maxy = (ay > by ? ay : by) + r;
    }
    break;
    case ROGUE_HITBOX_ARC:
    {
        float r = h->u.arc.radius;
        float ox = h->u.arc.ox, oy = h->u.arc.oy;
        *minx = ox - r;
        *maxx = ox + r;
        *miny = oy - r;
        *maxy = oy + r;
    }
    break;
    case ROGUE_HITBOX_CHAIN:
    {
        if (h->u.chain.count > 0)
        {
            float r = h->u.chain.radius;
            for (int i = 0; i < h->u.chain.count; i++)
            {
                float x = h->u.chain.px[i], y = h->u.chain.py[i];
                if (x - r < *minx)
                    *minx = x - r;
                if (x + r > *maxx)
                    *maxx = x + r;
                if (y - r < *miny)
                    *miny = y - r;
                if (y + r > *maxy)
                    *maxy = y + r;
            }
        }
    }
    break;
    case ROGUE_HITBOX_PROJECTILE_SPAWN:
    default:
        *minx = *miny = *maxx = *maxy = 0;
        break;
    }
}

int rogue_hitbox_load_directory(const char* dir, RogueHitbox* out, int max_out, int* out_count)
{
    if (out_count)
        *out_count = 0;
    if (!dir || !out || max_out <= 0)
        return 0;
    int total = 0;
#ifdef _WIN32
    struct _finddata_t fd;
    char pattern[512];
    snprintf(pattern, sizeof pattern, "%s/*.*", dir);
    intptr_t h = _findfirst(pattern, &fd);
    if (h == -1)
        return 0;
    do
    {
        if (fd.attrib & _A_SUBDIR)
            continue;
        const char* name = fd.name;
        size_t len = strlen(name);
        if (len < 5)
            continue;
        int ok = 0;
        if (len >= 7 && _stricmp(name + len - 7, ".hitbox") == 0)
            ok = 1;
        else if (len >= 5 && _stricmp(name + len - 5, ".json") == 0)
            ok = 1;
        if (!ok)
            continue;
        if (total >= max_out)
            break;
        char full[600];
        snprintf(full, sizeof full, "%s/%s", dir, name);
        int tmp_count = 0;
        if (!rogue_hitbox_load_sequence_file(full, out + total, max_out - total, &tmp_count))
            continue;
        total += tmp_count;
        if (total >= max_out)
            break;
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* d = opendir(dir);
    if (!d)
        return 0;
    struct dirent* ent;
    while ((ent = readdir(d)))
    {
        if (ent->d_type == DT_DIR)
            continue;
        const char* name = ent->d_name;
        size_t len = strlen(name);
        if (len < 5)
            continue;
        int ok = 0;
        if (len >= 7 && strcasecmp(name + len - 7, ".hitbox") == 0)
            ok = 1;
        else if (len >= 5 && strcasecmp(name + len - 5, ".json") == 0)
            ok = 1;
        if (!ok)
            continue;
        if (total >= max_out)
            break;
        char full[600];
        snprintf(full, sizeof full, "%s/%s", dir, name);
        int tmp_count = 0;
        if (!rogue_hitbox_load_sequence_file(full, out + total, max_out - total, &tmp_count))
            continue;
        total += tmp_count;
        if (total >= max_out)
            break;
    }
    closedir(d);
#endif
    if (out_count)
        *out_count = total;
    return 1;
}

int rogue_hitbox_collect_point_overlaps(const RogueHitbox* h, const float* xs, const float* ys,
                                        const int* alive, int count, int* out_indices, int max_out)
{
    if (!h || !xs || !ys || !out_indices || max_out <= 0)
        return 0;
    float minx, miny, maxx, maxy;
    hitbox_bounds(h, &minx, &miny, &maxx, &maxy);
    int n = 0;
    for (int i = 0; i < count; i++)
    {
        if (alive && !alive[i])
            continue;
        float x = xs[i], y = ys[i];
        if (x < minx || x > maxx || y < miny || y > maxy)
            continue;
        if (rogue_hitbox_point_overlap(h, x, y))
        {
            if (n < max_out)
                out_indices[n++] = i;
        }
    }
    return n;
}
