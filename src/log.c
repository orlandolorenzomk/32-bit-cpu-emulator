//
// Created by dev on 2/1/26.
//

#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

/* ANSI color codes */
#define CLR_RESET "\x1b[0m"
#define CLR_RED "\x1b[31m"
#define CLR_GREEN "\x1b[32m"
#define CLR_YELLOW "\x1b[33m"
#define CLR_BLUE "\x1b[34m"
#define CLR_MAGENTA "\x1b[35m"
#define CLR_CYAN "\x1b[36m"

/* global flags */
bool LOG_INFO_SHOW = true;
bool LOG_DEBUG_SHOW = true;
bool LOG_WARN_SHOW = true;
bool LOG_TRACE_SHOW = true;
bool LOG_ERROR_SHOW = true;
bool LOG_UNAUTHORIZED_SHOW = true;

/**
 * @brief Map a LogLevel to a human-readable string.
 *
 * Internal helper used by log_write to render a short textual level
 * prefix (e.g. "INFO", "DEBUG").
 *
 * @param level LogLevel value to map.
 * @return Pointer to a constant string for the given level.
 */
static const char *level_str(const LogLevel level)
{
    switch (level)
    {
    case LOG_INFO:
        return "INFO";
    case LOG_DEBUG:
        return "DEBUG";
    case LOG_WARN:
        return "WARN";
    case LOG_TRACE:
        return "TRACE";
    case LOG_ERROR:
        return "ERROR";
    case LOG_UNAUTHORIZED:
        return "UNAUTHORIZED";
    default:
        return "UNK";
    }
}

/**
 * @brief Map a LogLevel to an ANSI color escape sequence.
 *
 * Internal helper used to colorize log output on terminals that support
 * ANSI color codes. Returns CLR_RESET for unknown levels.
 *
 * @param level LogLevel value to map.
 * @return Pointer to constant string containing the ANSI color code.
 */
static const char *level_color(const LogLevel level)
{
    switch (level)
    {
    case LOG_INFO:
        return CLR_GREEN;
    case LOG_DEBUG:
        return CLR_CYAN;
    case LOG_WARN:
        return CLR_YELLOW;
    case LOG_TRACE:
        return CLR_BLUE;
    case LOG_ERROR:
        return CLR_RED;
    case LOG_UNAUTHORIZED:
        return CLR_MAGENTA;
    default:
        return CLR_RESET;
    }
}

/**
 * @brief Determine whether messages of the given level should be shown.
 *
 * The decision is driven by module-global booleans (e.g. LOG_DEBUG_SHOW)
 * which can be toggled by test harnesses or runtime configuration.
 *
 * @param level LogLevel to query.
 * @return true if messages of `level` should be printed, false otherwise.
 */
static bool should_show(const LogLevel level)
{
    switch (level)
    {
    case LOG_INFO:
        return LOG_INFO_SHOW;
    case LOG_DEBUG:
        return LOG_DEBUG_SHOW;
    case LOG_WARN:
        return LOG_WARN_SHOW;
    case LOG_TRACE:
        return LOG_TRACE_SHOW;
    case LOG_ERROR:
        return LOG_ERROR_SHOW;
    case LOG_UNAUTHORIZED:
        return LOG_UNAUTHORIZED_SHOW;
    default:
        return false;
    }
}

/**
 * @brief Format and print a log message to stdout with timestamp and level.
 *
 * The function first checks whether the given `level` should be shown via
 * `should_show`. If so it prints an ISO-like timestamp, a colored level
 * tag, then the formatted message. Uses va_list/printf style formatting.
 *
 * @param level Log level for the message.
 * @param fmt printf-style format string followed by optional arguments.
 */
void log_write(const LogLevel level, const char *fmt, ...)
{
    if (!should_show(level))
        return;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    printf("%04d-%02d-%02d %02d:%02d:%02d %s[%s]%s ",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
           tm.tm_hour, tm.tm_min, tm.tm_sec,
           level_color(level),
           level_str(level),
           CLR_RESET);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}
