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

#define ROGUE_LOG_DEBUG(fmt, ...)                                                                  \
    rogue_log(ROGUE_LOG_DEBUG_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ROGUE_LOG_INFO(fmt, ...)                                                                   \
    rogue_log(ROGUE_LOG_INFO_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ROGUE_LOG_WARN(fmt, ...)                                                                   \
    rogue_log(ROGUE_LOG_WARN_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ROGUE_LOG_ERROR(fmt, ...)                                                                  \
    rogue_log(ROGUE_LOG_ERROR_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
