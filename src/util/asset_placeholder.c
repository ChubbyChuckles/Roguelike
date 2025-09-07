/* Implementation: placeholder asset enforcement */
#include "asset_placeholder.h"
#include <stdio.h>

#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif
#endif

static int path_exists_single(const char* p)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, p, "rb") != 0)
        f = NULL;
#else
    f = fopen(p, "rb");
#endif
    if (f)
    {
        fclose(f);
        return 1;
    }
    return 0;
}

int rogue_asset_path_exists(const char* path)
{
    if (!path)
        return 0;
    return path_exists_single(path);
}

int rogue_asset_placeholder_exists(void)
{
    const char* rel = ROGUE_ASSET_PLACEHOLDER_PATH;
    /* Mirror the fallback search pattern used by some loaders so tests pass when
       executed from build/ or deeper working directories. */
    const char* prefixes[] = {"", "../", "../../", "../../../"};
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
    {
        char buf[512];
        int n = snprintf(buf, sizeof buf, "%s%s", prefixes[i], rel);
        if (n > 0 && n < (int) sizeof(buf))
        {
            if (path_exists_single(buf))
                return 1;
        }
    }
    return 0;
}
