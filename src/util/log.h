/**
 * @file log.h
 * @brief Logging system for debug output and diagnostics.
 *
 * Provides a comprehensive logging system with multiple severity levels,
 * configurable output filtering, and convenient macros for logging
 * messages throughout the application. Supports environment variable
 * configuration and automatic file/line information inclusion.
 *
 * @author [Your Name]
 * @date September 2025
 * @version 1.0
 */

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

/**
 * @brief Logging severity levels.
 *
 * Defines the different levels of log message severity, from debug
 * information to critical errors. Higher numbered levels indicate
 * more severe messages.
 */
typedef enum RogueLogLevel
{
    ROGUE_LOG_DEBUG_LEVEL, ///< Debug information (most verbose)
    ROGUE_LOG_INFO_LEVEL,  ///< General information messages  
    ROGUE_LOG_WARN_LEVEL,  ///< Warning messages (potential issues)
    ROGUE_LOG_ERROR_LEVEL  ///< Error messages (serious problems)
} RogueLogLevel;

/**
 * @brief Core logging function.
 *
 * Outputs a formatted log message with the specified severity level,
 * including file and line information. Messages below the current
 * log level threshold are filtered out.
 *
 * @param level Severity level of the message
 * @param file Source file name (typically __FILE__)
 * @param line Source line number (typically __LINE__)
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void rogue_log(RogueLogLevel level, const char* file, int line, const char* fmt, ...);

/**
 * @brief Set the minimum log level threshold.
 *
 * Configures the global log level filter. Messages with severity
 * below this threshold will be ignored and not output.
 *
 * @param min_level Minimum severity level to output
 */
void rogue_log_set_level(RogueLogLevel min_level);

/**
 * @brief Get the current log level threshold.
 *
 * Returns the currently configured minimum log level that
 * determines which messages are output.
 *
 * @return Current minimum log level threshold
 */
RogueLogLevel rogue_log_get_level(void);

/**
 * @brief Configure log level from environment variable.
 *
 * Reads the ROGUE_LOG_LEVEL environment variable and sets the
 * log level accordingly. Supports both string values (debug, info, 
 * warn, error) and numeric values (0-3).
 */
void rogue_log_set_level_from_env(void);

/**
 * @brief Log a debug message.
 *
 * Convenience macro for logging debug-level messages. Automatically
 * includes file and line information. Debug messages are typically
 * used for detailed diagnostic information during development.
 *
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define ROGUE_LOG_DEBUG(fmt, ...)                                                                  \
    rogue_log(ROGUE_LOG_DEBUG_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an information message.
 *
 * Convenience macro for logging informational messages. Used for
 * general status updates and non-critical information that may
 * be useful for understanding program flow.
 *
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define ROGUE_LOG_INFO(fmt, ...)                                                                   \
    rogue_log(ROGUE_LOG_INFO_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log a warning message.
 *
 * Convenience macro for logging warning messages. Used for potential
 * issues or unusual conditions that don't prevent operation but
 * may indicate problems.
 *
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define ROGUE_LOG_WARN(fmt, ...)                                                                   \
    rogue_log(ROGUE_LOG_WARN_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

/**
 * @brief Log an error message.
 *
 * Convenience macro for logging error messages. Used for serious
 * problems that prevent normal operation or indicate significant
 * failures in the system.
 *
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
#define ROGUE_LOG_ERROR(fmt, ...)                                                                  \
    rogue_log(ROGUE_LOG_ERROR_LEVEL, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif
