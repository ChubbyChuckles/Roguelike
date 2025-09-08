/* Common helpers for Asset Browser extracted modules */
#ifndef ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_UTIL_H
#define ROGUE_DEBUG_OVERLAY_ASSET_BROWSER_UTIL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
    /* Truncate string with ellipsis if length exceeds max_chars. Guarantees NUL termination. */
    void rogue_ab_truncate_ellipsis(char* dst, size_t cap, const char* src, int max_chars);
    /* Case-insensitive wildcard match supporting '*' and '?' tokens. Returns 1 on match. */
    int rogue_ab_match_wildcard_ci(const char* text, const char* pattern);
#ifdef __cplusplus
}
#endif
#endif
