#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <types.h>

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA    "\x1b[36m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef enum {
    LOG_TEST,
    LOG_TRACE,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR,
    LOG_SUPPRESSED,

    LOG_NUM_LEVELS,
} LogLevel;

// TODO: filename/line etc

#define LogOnce(level, format, ...) {\
    static bool __init = false;\
    if (!__init) {\
        __init = true;\
        _LogMessage(level, __FILE__, __LINE__, format, ##__VA_ARGS__);\
    }\
}

long long GetTimeMs();

#define LogPeriodic(level, periodMs, format, ...) {\
    static long long next = 0;\
    long long current = GetTimeMs();\
    if (current >= next) {\
        _LogMessage(level, __FILE__, __LINE__, format, ##__VA_ARGS__);\
        next = current + periodMs;\
    }\
}


#define LogTest(format, ...) _LogMessage(LOG_TEST, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LogTrace(format, ...) _LogMessage(LOG_TRACE, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LogInfo(format, ...) _LogMessage(LOG_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LogWarn(format, ...) _LogMessage(LOG_WARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define LogError(format, ...) _LogMessage(LOG_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)

#define LogTestOnce(format, ...) LogOnce(LOG_TEST, format, ##__VA_ARGS__)
#define LogTraceOnce(format, ...) LogOnce(LOG_TRACE, format, ##__VA_ARGS__)
#define LogInfoOnce(format, ...) LogOnce(LOG_INFO, format, ##__VA_ARGS__)
#define LogWarnOnce(format, ...) LogOnce(LOG_WARNING, format, ##__VA_ARGS__)
#define LogErrorOnce(format, ...) LogOnce(LOG_ERROR, format, ##__VA_ARGS__)

#define LogTestPeriodic(periodMs, format, ...) LogPeriodic(LOG_TEST, periodMs, format, ##__VA_ARGS__)
#define LogDebugPeriodic(periodMs, format, ...) LogPeriodic(LOG_TRACE, periodMs, format, ##__VA_ARGS__)
#define LogInfoPeriodic(periodMs, format, ...) LogPeriodic(LOG_INFO, periodMs, format, ##__VA_ARGS__)
#define LogWarnPeriodic(periodMs, format, ...) LogPeriodic(LOG_WARNING, periodMs, format, ##__VA_ARGS__)
#define LogErrorPeriodic(periodMs, format, ...) LogPeriodic(LOG_ERROR, periodMs, format, ##__VA_ARGS__)

#define LogInfoRaw(format, ...) _LogRaw(LOG_INFO, format, ##__VA_ARGS__)
#define LogWarnRaw(format, ...) _LogRaw(LOG_WARNING, format, ##__VA_ARGS__)
#define LogErrorRaw(format, ...) _LogRaw(LOG_ERROR, format, ##__VA_ARGS__)

#define Assert(cond, format, ...) _Assert(cond, #cond, __FILE__, __LINE__, format, ##__VA_ARGS__)

void _Assert(bool condition, const char* condString, const char* file, i32 line, const char* format, ...);
void _LogRaw(LogLevel level, const char* format, ...);
void _LogMessage(LogLevel level, const char* file, i32 line, const char* format, ...);

void RegisterAssertHandler(void (*handler)(char const*, const char*, i32));
void SetLogLevel(LogLevel level);
