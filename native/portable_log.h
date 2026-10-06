#pragma once

// Keep the package/cache/runtime implementation identical on Android and host.
#if defined(__ANDROID__)
#include <android/log.h>
#else
#include <cstdarg>
#include <cstdio>

constexpr int ANDROID_LOG_INFO = 4;
constexpr int ANDROID_LOG_WARN = 5;
constexpr int ANDROID_LOG_ERROR = 6;

inline int __android_log_print(int priority, const char* tag, const char* format, ...) {
    std::FILE* stream = priority >= ANDROID_LOG_WARN ? stderr : stdout;
    std::fprintf(stream, "[%s] ", tag);
    va_list arguments;
    va_start(arguments, format);
    const int count = std::vfprintf(stream, format, arguments);
    va_end(arguments);
    std::fputc('\n', stream);
    return count;
}
#endif
