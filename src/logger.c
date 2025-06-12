#include <logger.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

#include <core_engine.h>

static const char* levels_[LOG_NUM_LEVELS] = {
    "INFO",
    "WARN",
    "ERROR",
};

static const char* colors_[LOG_NUM_LEVELS] = {
    ANSI_COLOR_RESET, // INFO
    ANSI_COLOR_YELLOW, // WARN
    ANSI_COLOR_RED, // ERROR
};

static void DefaultAssertHandler(const char* message, const char* file, i32 line);

static LogLevel currentLevel_ = LOG_INFO;
static void (*assertHandler_)(const char*, const char*, i32) = DefaultAssertHandler;

static void DefaultAssertHandler(const char* message, const char* file, i32 line)
{
    fprintf(stderr, "%s %s:%d\n", message, file , line);
    exit(1);
}

void RegisterAssertHandler(void (*handler)(const char *, const char *, i32))
{
    assertHandler_ = handler;
}

long long GetTimeMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + (long long)tv.tv_usec / 1000;
}

void _LogMessage(LogLevel level, const char* format, ...)
{
    if (level < currentLevel_) {
        return;
    }

    // Format a timestamp
    char timeBuf[30];
    int ms;
    struct tm* tmInfo;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ms = lrint(tv.tv_usec / 1000.0);
    if (ms >= 1000.0) {
        ms -= 1000.0;
        tv.tv_sec++;
    }
    tmInfo = localtime(&tv.tv_sec);
    strftime(timeBuf, 26, "%H:%M:%S", tmInfo);
    snprintf(timeBuf + strlen(timeBuf), sizeof(timeBuf) - strlen(timeBuf), ":%03d", ms);

    // Format the output buffer
    char formatBuf[256];
    va_list args;
    va_start(args, format);
    sprintf(formatBuf, "%s[%s] [%s] -- %s%s\n", colors_[level], levels_[level], timeBuf, format, ANSI_COLOR_RESET);
    vprintf(formatBuf, args);
    va_end(args);
}

void _LogRaw(LogLevel level, const char *format, ...)
{
    if (level < currentLevel_) {
        return;
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void _Assert(bool condition, const char* condString, const char* file, i32 line, const char *format, ...)
{
    if (!condition)
    {
        char formatBuf[256];
        char outputBuf[256];
        va_list args;

        sprintf(formatBuf, "ASSERT FAILED -- %s (%s)", format, condString);
        va_start(args, format);
        vsprintf(outputBuf, formatBuf, args);
        va_end(args);

        assertHandler_(outputBuf, file, line);
    }
}

