/* path_utils.c - initial helpers (Phase 3) */
#include "path_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

void rogue_path_normalize(char* path)
{
    if (!path)
        return;
    for (char* p = path; *p; ++p)
    {
        if (*p == '\\')
            *p = '/';
    }
}

bool rogue_path_is_absolute(const char* path)
{
    if (!path || !*path)
        return false;
#ifdef _WIN32
    /* Drive letter or UNC */
    if (isalpha((unsigned char) path[0]) && path[1] == ':' && (path[2] == '/' || path[2] == '\\'))
        return true;
    if (path[0] == '\\' && path[1] == '\\')
        return true;
#else
    if (path[0] == '/')
        return true;
#endif
    return false;
}

bool rogue_path_join(const char* a, const char* b, char* out, int out_cap)
{
    if (!out || out_cap <= 0)
        return false;
    out[0] = '\0';
    if (!a && !b)
        return true;
    if (!b)
    {
        size_t la = strlen(a);
        if (la >= out_cap)
            la = out_cap - 1;
        memcpy(out, a, la);
        out[la] = '\0';
        rogue_path_normalize(out);
        return true;
    }
    if (!a)
    {
        size_t lb = strlen(b);
        if (lb >= out_cap)
            lb = out_cap - 1;
        memcpy(out, b, lb);
        out[lb] = '\0';
        rogue_path_normalize(out);
        return true;
    }
    size_t la = strlen(a);
    int need_sep = (la > 0 && a[la - 1] != '/' && a[la - 1] != '\\');
    snprintf(out, out_cap, "%s%s%s", a, need_sep ? "/" : "", b);
    rogue_path_normalize(out);
    return true;
}
