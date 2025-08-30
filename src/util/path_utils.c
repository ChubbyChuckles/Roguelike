/**
 * @file path_utils.c
 * @brief Utility functions for finding asset and documentation file paths.
 * @details This module provides functions to locate asset files and documentation
 * files by searching through common directory prefixes relative to the executable.
 */

#include "path_utils.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Finds the full path to an asset file by searching common directories.
 * @param filename The name of the asset file to find.
 * @param out Buffer to store the found path.
 * @param out_sz Size of the output buffer.
 * @return 1 if the file was found and path copied to out, 0 otherwise.
 * @details Searches for the file in "assets/", "../assets/", "../../assets/",
 * and "../../../assets/" directories in order. Uses platform-specific file
 * operations for MSVC vs POSIX compatibility.
 */
int rogue_find_asset_path(const char* filename, char* out, int out_sz)
{
    if (!filename || !out || out_sz <= 0)
        return 0;
    const char* prefixes[] = {"assets/", "../assets/", "../../assets/", "../../../assets/"};
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
    /* silent failure */
    return 0;
}

/**
 * @brief Finds the full path to a documentation file by searching common directories.
 * @param filename The name of the documentation file to find.
 * @param out Buffer to store the found path.
 * @param out_sz Size of the output buffer.
 * @return 1 if the file was found and path copied to out, 0 otherwise.
 * @details Searches for the file in "docs/", "../docs/", "../../docs/",
 * and "../../../docs/" directories in order. Uses platform-specific file
 * operations for MSVC vs POSIX compatibility.
 */
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
