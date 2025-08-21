/* Implementation of recursive filename search used for robust fallback when configured
   asset paths are invalid. Designed to be lightweight and avoid dynamic allocations. */
#include "util/file_search.h"
#include "util/log.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define ROGUE_PATH_SEP '\\'
#else
#include <dirent.h>
#define ROGUE_PATH_SEP '/'
#endif

static int rogue__name_match(const char* a, const char* b)
{
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}

static int rogue__search_dir(const char* dir, const char* target, char* out, int out_sz, int depth,
                             int max_depth)
{
    if (depth > max_depth)
        return 0;
#ifdef _WIN32
    char pattern[640];
    snprintf(pattern, sizeof pattern, "%s/*", dir);
    struct _finddata_t fd;
    intptr_t h = _findfirst(pattern, &fd);
    if (h == -1)
        return 0;
    do
    {
        const char* name = fd.name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (rogue__name_match(name, target))
        {
            snprintf(out, out_sz, "%s%c%s", dir, ROGUE_PATH_SEP, name);
            _findclose(h);
            return 1;
        }
        if (fd.attrib & _A_SUBDIR)
        {
            char child[640];
            snprintf(child, sizeof child, "%s%c%s", dir, ROGUE_PATH_SEP, name);
            if (rogue__search_dir(child, target, out, out_sz, depth + 1, max_depth))
            {
                _findclose(h);
                return 1;
            }
        }
    } while (_findnext(h, &fd) == 0);
    _findclose(h);
#else
    DIR* d = opendir(dir);
    if (!d)
        return 0;
    struct dirent* ent;
    while ((ent = readdir(d)))
    {
        const char* name = ent->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
            continue;
        if (rogue__name_match(name, target))
        {
            snprintf(out, out_sz, "%s/%s", dir, name);
            closedir(d);
            return 1;
        }
        int is_dir = 0;
#ifdef _DIRENT_HAVE_D_TYPE
        if (ent->d_type == DT_DIR)
            is_dir = 1;
        else if (ent->d_type == DT_UNKNOWN)
            is_dir = 0; /* could stat if needed */
#endif
        if (is_dir)
        {
            char child[640];
            snprintf(child, sizeof child, "%s/%s", dir, name);
            if (rogue__search_dir(child, target, out, out_sz, depth + 1, max_depth))
            {
                closedir(d);
                return 1;
            }
        }
    }
    closedir(d);
#endif
    return 0;
}

int rogue_file_search_project(const char* target_name, char* out, int out_sz)
{
    if (!target_name || !*target_name || !out || out_sz <= 0)
        return 0;
    out[0] = '\0';
    const char* roots[] = {".", "..", "../..", "../../.."};
    const int max_depth = 18; /* safeguard to prevent runaway recursion */
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
    {
        if (rogue__search_dir(roots[i], target_name, out, out_sz, 0, max_depth))
            return 1;
    }
    return 0;
}
