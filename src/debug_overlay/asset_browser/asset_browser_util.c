#include "asset_browser_util.h"
#include <string.h>
void rogue_ab_truncate_ellipsis(char* dst, size_t cap, const char* src, int max_chars)
{
    if (!dst || cap == 0)
        return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    if (max_chars < 5)
        max_chars = 5;
    int len = (int) strlen(src);
    if (len < max_chars)
    {
        size_t i = 0;
        while (src[i] && i + 1 < cap)
        {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
        return;
    }
    int copy_len = max_chars - 3;
    if ((size_t) copy_len + 4 > cap)
        copy_len = (int) cap - 4;
    if (copy_len < 1)
    {
        dst[0] = '\0';
        return;
    }
    memcpy(dst, src, (size_t) copy_len);
    dst[copy_len] = '.';
    dst[copy_len + 1] = '.';
    dst[copy_len + 2] = '.';
    dst[copy_len + 3] = '\0';
}
int rogue_ab_match_wildcard_ci(const char* text, const char* pattern)
{
    if (!pattern || !pattern[0])
        return 1;
    if (!text)
        return 0;
    const char *t = text, *p = pattern;
    const char *star = NULL, *star_text = NULL;
    while (*t)
    {
        char pc = *p;
        if (pc == '*')
        {
            star = p++;
            star_text = t;
            continue;
        }
        if (pc == '?' ||
            (pc && (char) tolower((unsigned char) pc) == (char) tolower((unsigned char) *t)))
        {
            p++;
            t++;
            continue;
        }
        if (star)
        {
            p = star + 1;
            t = ++star_text;
            continue;
        }
        return 0;
    }
    while (*p == '*')
        p++;
    return *p == '\0';
}
/* Legacy compatibility wrapper: old symbol name kept to avoid widespread churn during
   incremental refactor. Remove once all references updated. */
int ab_match_wildcard_ci(const char* text, const char* pattern)
{
    return rogue_ab_match_wildcard_ci(text, pattern);
}
