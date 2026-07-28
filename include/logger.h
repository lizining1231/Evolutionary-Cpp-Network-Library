#ifndef LOGGER_H
#define LOGGER_H

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

enum LogLevel {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
};

#ifndef LOG_LEVEL
#ifdef NDEBUG
#define LOG_LEVEL 0  // NONE
#else
#define LOG_LEVEL 3  // INFO
#endif
#endif

inline void log_impl(FILE* out, const char* level, const char* file, int line, const char* fmt,
                     ...) {
    char timestamp[64] = {0};
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(timestamp, 64, "%Y-%m-%d %H:%M:%S", &tm);

    char buf[4096];
    const char* filename = strrchr(file, '/');
    int n = snprintf(buf, sizeof(buf), "[%s] [%s] [%s:%d] ", timestamp, level,
                     filename ? filename + 1 : file, line);

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);

    fwrite(buf, 1, strlen(buf), out);
    fputc('\n', out);
}

#if LOG_LEVEL >= 4
#define LOG_DEBUG(...) log_impl(stdout, "DEBUG", __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0);
#endif

#if LOG_LEVEL >= 3
#define LOG_INFO(...) log_impl(stdout, "INFO", __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_INFO(...) ((void)0);
#endif

#if LOG_LEVEL >= 2
#define LOG_WARN(...) log_impl(stdout, "WARN", __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_WARN(...) ((void)0);
#endif

#if LOG_LEVEL >= 1
#define LOG_ERROR(...) log_impl(stderr, "ERROR", __FILE__, __LINE__, __VA_ARGS__)
#else
#define LOG_ERROR(...) ((void)0);
#endif

#endif  // LOGGER_H
