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
#include "util/log.h"
#include <stdarg.h>

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

void rogue_log(RogueLogLevel level, const char* file, int line, const char* fmt, ...)
{
    FILE* out = (level == ROGUE_LOG_ERROR_LEVEL) ? stderr : stdout;
    fprintf(out, "[%s] %s:%d: ", level_to_str(level), file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    fprintf(out, "\n");
}
