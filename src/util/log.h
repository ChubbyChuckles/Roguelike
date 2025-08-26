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
#ifndef ROGUE_UTIL_LOG_H
#define ROGUE_UTIL_LOG_H

#include <stdio.h>

typedef enum RogueLogLevel
{
    ROGUE_LOG_DEBUG_LEVEL,
    ROGUE_LOG_INFO_LEVEL,
    ROGUE_LOG_WARN_LEVEL,
    ROGUE_LOG_ERROR_LEVEL
} RogueLogLevel;

void rogue_log(RogueLogLevel level, const char* file, int line, const char* fmt, ...);

/* Configure global log level threshold (messages below are ignored). */
void rogue_log_set_level(RogueLogLevel min_level);
RogueLogLevel rogue_log_get_level(void);
/* Convenience: reads ROGUE_LOG_LEVEL env var: debug|info|warn|error or 0..3 */
void rogue_log_set_level_from_env(void);

#define ROGUE_LOG_DEBUG(fmt, ...)                                                                  \
    rogue_log(ROGUE_LOG_DEBUG_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ROGUE_LOG_INFO(fmt, ...)                                                                   \
    rogue_log(ROGUE_LOG_INFO_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ROGUE_LOG_WARN(fmt, ...)                                                                   \
    rogue_log(ROGUE_LOG_WARN_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ROGUE_LOG_ERROR(fmt, ...)                                                                  \
    rogue_log(ROGUE_LOG_ERROR_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
