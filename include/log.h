//
// Created by dev on 2/1/26.
//

#ifndef INC_8BIT_CPU_EMULATOR_LOG_H
#define INC_8BIT_CPU_EMULATOR_LOG_H

#define unscast unsigned // Use in formatters to avoid platform-specific compiler warnings

/**
 * @enum LogLevel
 * @brief Logging verbosity levels used by log_write().
 *
 * Levels map to severity and are used to decide whether to print messages
 * based on module-global flags (for example LOG_DEBUG_SHOW).
 */
typedef enum
{
    LOG_INFO,
    LOG_DEBUG,
    LOG_WARN,
    LOG_TRACE,
    LOG_ERROR,
    LOG_UNAUTHORIZED
} LogLevel;

/**
 * @brief Write a formatted log message.
 *
 * The function prints a timestamped, colored message to stdout depending on
 * the `level`. The message is formatted like printf using the provided
 * format string and additional arguments.
 *
 * @param level Severity level for the log message.
 * @param fmt printf-style format string followed by arguments.
 */
void log_write(LogLevel level, const char *fmt, ...);


#endif //INC_8BIT_CPU_EMULATOR_LOG_H