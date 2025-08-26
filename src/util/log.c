/*
MIT License

Copyright (c) 2025 ChubbyChuckles

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static const char* level_to_str(RogueLogLevel lvl)
{
    switch (lvl)
    {
    case ROGUE_LOG_DEBUG_LEVEL:
        return "DEBUG";
    case ROGUE_LOG_INFO_LEVEL:
        return "INFO";
    case ROGUE_LOG_WARN_LEVEL:
        return "WARN";
    case ROGUE_LOG_ERROR_LEVEL:
        return "ERROR";
    default:
        return "?";
    }
}

/* Internal linkage for single TU static state */
static RogueLogLevel s_global_level = ROGUE_LOG_WARN_LEVEL;

/* Platform helpers */
static const char* rogue_getenv_safe(const char* name)
{
#ifdef _MSC_VER
    static char buf[128];
    char* tmp = NULL;
    size_t len = 0;
    if (_dupenv_s(&tmp, &len, name) == 0 && tmp && *tmp)
    {
        strncpy_s(buf, sizeof(buf), tmp, _TRUNCATE);
        free(tmp);
        return buf;
    }
    if (tmp)
        free(tmp);
    return NULL;
#else
    return getenv(name);
#endif
}

static int rogue_stricmp(const char* a, const char* b)
{
#ifdef _MSC_VER
    return _stricmp(a, b);
#else
    /* simple portable case-insensitive compare */
    unsigned char ca, cb;
    while (*a && *b)
    {
        ca = (unsigned char) *a;
        cb = (unsigned char) *b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (unsigned char) (ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (unsigned char) (cb - 'A' + 'a');
        if (ca != cb)
            return (int) ca - (int) cb;
        ++a;
        ++b;
    }
    return (int) ((unsigned char) *a) - (int) ((unsigned char) *b);
#endif
}

void rogue_log(RogueLogLevel level, const char* file, int line, const char* fmt, ...)
{
    static int s_init = 0;
    if (!s_init)
    {
        /* allow env override on first use */
        const char* e = rogue_getenv_safe("ROGUE_LOG_LEVEL");
        if (e && *e)
        {
            if (!rogue_stricmp(e, "debug") || !strcmp(e, "0"))
                s_global_level = ROGUE_LOG_DEBUG_LEVEL;
            else if (!rogue_stricmp(e, "info") || !strcmp(e, "1"))
                s_global_level = ROGUE_LOG_INFO_LEVEL;
            else if (!rogue_stricmp(e, "warn") || !rogue_stricmp(e, "warning") || !strcmp(e, "2"))
                s_global_level = ROGUE_LOG_WARN_LEVEL;
            else if (!rogue_stricmp(e, "error") || !strcmp(e, "3"))
                s_global_level = ROGUE_LOG_ERROR_LEVEL;
        }
        s_init = 1;
    }
    if (level < s_global_level)
        return;
    FILE* out = (level == ROGUE_LOG_ERROR_LEVEL) ? stderr : stdout;
    fprintf(out, "[%s] %s:%d: ", level_to_str(level), file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fprintf(out, "\n");
}

void rogue_log_set_level(RogueLogLevel min_level) { s_global_level = min_level; }
RogueLogLevel rogue_log_get_level(void) { return s_global_level; }

void rogue_log_set_level_from_env(void)
{
    const char* e = rogue_getenv_safe("ROGUE_LOG_LEVEL");
    if (!e || !*e)
        return;
    if (!rogue_stricmp(e, "debug") || !strcmp(e, "0"))
        rogue_log_set_level(ROGUE_LOG_DEBUG_LEVEL);
    else if (!rogue_stricmp(e, "info") || !strcmp(e, "1"))
        rogue_log_set_level(ROGUE_LOG_INFO_LEVEL);
    else if (!rogue_stricmp(e, "warn") || !rogue_stricmp(e, "warning") || !strcmp(e, "2"))
        rogue_log_set_level(ROGUE_LOG_WARN_LEVEL);
    else if (!rogue_stricmp(e, "error") || !strcmp(e, "3"))
        rogue_log_set_level(ROGUE_LOG_ERROR_LEVEL);
}
