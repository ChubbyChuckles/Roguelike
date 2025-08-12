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
