#include "path_utils.h"
#include <stdio.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif

int rogue_find_asset_path(const char* filename, char* out, int out_sz)
{
    if (!filename || !out || out_sz <= 0)
        return 0;

    /* Build dynamic prefix list. We optionally prepend SDL_GetBasePath derived prefixes
       (base/, base/../, base/../../) before the legacy relative probes so test executables
       launched from nested build output dirs can still discover the repo root assets/. */
    const char* static_rel[] = {"assets/", "../assets/", "../../assets/", "../../../assets/"};
    const size_t static_rel_count = sizeof(static_rel) / sizeof(static_rel[0]);

#ifdef ROGUE_HAVE_SDL
    static char base_cache[512];
    if (base_cache[0] == '\0')
    {
        char* bp = SDL_GetBasePath();
        if (bp)
        {
            /* Copy (may include trailing path separator) */
#if defined(_MSC_VER)
            strncpy_s(base_cache, sizeof(base_cache), bp, _TRUNCATE);
#else
            strncpy(base_cache, bp, sizeof(base_cache) - 1);
            base_cache[sizeof(base_cache) - 1] = '\0';
#endif
            SDL_free(bp);
        }
    }
#endif

    /* We'll try at most 3 base path ascents ("", "../", "../../") if base path known. */
    char test[512];
#ifdef ROGUE_HAVE_SDL
    if (base_cache[0])
    {
        const char* ascents[] = {"", "../", "../../"};
        for (size_t a = 0; a < sizeof(ascents) / sizeof(ascents[0]); ++a)
        {
            snprintf(test, sizeof test, "%s%sassets/%s", base_cache, ascents[a], filename);
            FILE* f = NULL;
#if defined(_MSC_VER)
            fopen_s(&f, test, "rb");
#else
            f = fopen(test, "rb");
#endif
            if (f)
            {
                fclose(f);
#if defined(_MSC_VER)
                strncpy_s(out, out_sz, test, _TRUNCATE);
#else
                strncpy(out, test, out_sz - 1);
                out[out_sz - 1] = '\0';
#endif
                return 1;
            }
        }
    }
#endif

    for (size_t i = 0; i < static_rel_count; ++i)
    {
        snprintf(test, sizeof test, "%s%s", static_rel[i], filename);
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, test, "rb");
#else
        f = fopen(test, "rb");
#endif
        if (f)
        {
            fclose(f);
#if defined(_MSC_VER)
            strncpy_s(out, out_sz, test, _TRUNCATE);
#else
            strncpy(out, test, out_sz - 1);
            out[out_sz - 1] = '\0';
#endif
            return 1;
        }
    }
    return 0; /* silent failure */
}

int rogue_find_doc_path(const char* filename, char* out, int out_sz)
{
    if (!filename || !out || out_sz <= 0)
        return 0;
    const char* prefixes[] = {"docs/", "../docs/", "../../docs/", "../../../docs/"};
    char test[512];
    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
    {
        snprintf(test, sizeof test, "%s%s", prefixes[i], filename);
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, test, "rb");
#else
        f = fopen(test, "rb");
#endif
        if (f)
        {
            fclose(f);
#if defined(_MSC_VER)
            strncpy_s(out, out_sz, test, _TRUNCATE);
#else
            strncpy(out, test, out_sz - 1);
            out[out_sz - 1] = '\0';
#endif
            return 1;
        }
    }
    return 0;
}
