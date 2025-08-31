#include "dialogue_util.h"
#include "../util/path_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* jd_skip_ws(const char* s)
{
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        ++s;
    return s;
}
static unsigned int jd_hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return (unsigned int) (c - '0');
    if (c >= 'a' && c <= 'f')
        return 10u + (unsigned int) (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10u + (unsigned int) (c - 'A');
    return 0u;
}

int rogue_dialogue__parse_color(const char* s, unsigned int* out)
{
    if (!s || !out)
        return -1;
    if (s[0] == '#')
    {
        size_t len = strlen(s);
        if (len == 7)
        {
            unsigned int v = 0;
            for (int i = 1; i < 7; i++)
            {
                v = (v << 4) | jd_hex_nibble(s[i]);
            }
            *out = 0xFF000000u | v;
            return 0;
        }
    }
    else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        unsigned int v = 0;
        for (int i = 2; s[i] && i < 10; ++i)
        {
            char c = s[i];
            if (!isxdigit((unsigned char) c))
                break;
            v = (v << 4) | jd_hex_nibble(c);
        }
        *out = v;
        return 0;
    }
    else
    { /* decimal */
        unsigned int v = 0;
        for (int i = 0; s[i]; ++i)
        {
            if (s[i] < '0' || s[i] > '9')
                break;
            v = v * 10u + (unsigned int) (s[i] - '0');
        }
        *out = v;
        return 0;
    }
    return -1;
}

int rogue_dialogue__json_extract_string(const char* json, const char* key, char* out, size_t cap)
{
    const char* search = json;
    size_t key_len = strlen(key);
    while (1)
    {
        const char* found = strstr(search, key);
        if (!found)
            return -1;
        search = found + key_len;
        const char* colon = strchr(found, ':');
        if (!colon)
            continue;
        const char* v = jd_skip_ws(colon + 1);
        if (*v != '"')
            continue;
        v++;
        const char* end = strchr(v, '"');
        if (!end)
            return -1;
        size_t len = (size_t) (end - v);
        if (len > cap - 1)
            len = cap - 1;
        memcpy(out, v, len);
        out[len] = '\0';
        return 0;
    }
}

int rogue_dialogue__json_extract_int(const char* json, const char* key, int* out)
{
    const char* search = json;
    size_t key_len = strlen(key);
    while (1)
    {
        const char* found = strstr(search, key);
        if (!found)
            return -1;
        search = found + key_len;
        const char* colon = strchr(found, ':');
        if (!colon)
            continue;
        const char* v = jd_skip_ws(colon + 1);
        int sign = 1;
        if (*v == '-')
        {
            sign = -1;
            v++;
        }
        if (!isdigit((unsigned char) *v))
            continue;
        int val = 0;
        while (isdigit((unsigned char) *v))
        {
            val = val * 10 + (*v - '0');
            v++;
        }
        *out = val * sign;
        return 0;
    }
}

int rogue_dialogue__read_all(const char* path, char** out_buf, int* out_len)
{
    if (!path || !out_buf || !out_len)
        return -10;
    FILE* f = NULL;
#if defined(_MSC_VER)
    int fopen_result = fopen_s(&f, path, "rb");
    if (fopen_result != 0 || !f)
    {
        if (path && strncmp(path, "assets/", 7) == 0)
        {
            char resolved[512];
            if (rogue_find_asset_path(path + 7, resolved, (int) sizeof resolved))
            {
                if (fopen_s(&f, resolved, "rb") != 0 || !f)
                    return -1;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
#else
    f = fopen(path, "rb");
    if (!f)
    {
        if (path && strncmp(path, "assets/", 7) == 0)
        {
            char resolved[512];
            if (rogue_find_asset_path(path + 7, resolved, (int) sizeof resolved))
            {
                f = fopen(resolved, "rb");
                if (!f)
                    return -1;
            }
            else
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
    }
#endif
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -2;
    }
    long sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return -3;
    }
    rewind(f);
    char* buf = (char*) malloc((size_t) sz + 1);
    if (!buf)
    {
        fclose(f);
        return -4;
    }
    size_t rd = fread(buf, 1, (size_t) sz, f);
    fclose(f);
    buf[rd] = '\0';
    *out_buf = buf;
    *out_len = (int) rd;
    return 0;
}
