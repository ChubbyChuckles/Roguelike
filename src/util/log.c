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

/**
 * @file log.c
 * @brief Cross-platform logging utility with configurable levels and environment override.
 * @details This module provides a simple logging system with different severity levels,
 * platform-specific implementations for MSVC vs POSIX, and environment variable configuration.
 */

#include "log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Converts a log level enum to its string representation.
 * @param lvl The log level to convert.
 * @return String representation of the log level.
 * @details Used internally for formatting log messages.
 */
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

/**
 * @brief Safely retrieves an environment variable value.
 * @param name The name of the environment variable.
 * @return The value of the environment variable, or NULL if not found.
 * @details Uses platform-specific functions for safe environment variable access.
 */
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

/**
 * @brief Performs case-insensitive string comparison.
 * @param a First string to compare.
 * @param b Second string to compare.
 * @return Negative if a < b, 0 if equal, positive if a > b (case-insensitive).
 * @details Provides portable case-insensitive comparison for MSVC and POSIX.
 */
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

/**
 * @brief Logs a message with the specified level, file, and line information.
 * @param level The severity level of the log message.
 * @param file The source file name where the log call originated.
 * @param line The line number where the log call originated.
 * @param fmt Format string (printf-style).
 * @param ... Variable arguments for the format string.
 * @details Checks the global log level and only outputs if the message level meets the threshold.
 * Outputs to stderr for errors, stdout for other levels. Supports environment variable override on
 * first use.
 */
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

/**
 * @brief Sets the minimum log level for message output.
 * @param min_level The minimum severity level to output.
 * @details Messages below this level will be suppressed.
 */
void rogue_log_set_level(RogueLogLevel min_level) { s_global_level = min_level; }

/**
 * @brief Gets the current minimum log level.
 * @return The current minimum log level.
 * @details Returns the threshold level for log message output.
 */
RogueLogLevel rogue_log_get_level(void) { return s_global_level; }

/**
 * @brief Sets the log level from the ROGUE_LOG_LEVEL environment variable.
 * @details Reads and parses the ROGUE_LOG_LEVEL environment variable to set the log level.
 * Supports both string names (debug, info, warn, error) and numeric values (0-3).
 */
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
