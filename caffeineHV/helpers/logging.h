#pragma once

#include <ntddk.h>
#include <ntstrsafe.h>
#include <stdarg.h>

namespace Caffeine::Logging {
    enum class LogLevel : char {
        Info,
        Warn,
        Error,
        Success,
        Debug
    };

    inline LogLevel g_MinLogLevel = LogLevel::Debug;

    constexpr ULONG GetComponentId() {
        return DPFLTR_IHVDRIVER_ID;
    }

    constexpr ULONG GetLevel(LogLevel lvl) {
        switch (lvl) {
        case LogLevel::Error:   return DPFLTR_ERROR_LEVEL;
        case LogLevel::Warn:    return DPFLTR_WARNING_LEVEL;
        case LogLevel::Info:    return DPFLTR_INFO_LEVEL;
        case LogLevel::Success: return DPFLTR_INFO_LEVEL;
        case LogLevel::Debug:   return DPFLTR_TRACE_LEVEL;
        }
        return DPFLTR_INFO_LEVEL;
    }

    constexpr const char* GetPrefix(LogLevel lvl) {
        switch (lvl) {
        case LogLevel::Info:    return "[caffeineHV] [*] ";
        case LogLevel::Warn:    return "[caffeineHV] [!] ";
        case LogLevel::Error:   return "[caffeineHV] [-] ";
        case LogLevel::Success: return "[caffeineHV] [+] ";
        case LogLevel::Debug:   return "[caffeineHV] [D] ";
        }
        return "";
    }

    inline bool ShouldLog(LogLevel lvl) {
        // Use kernel-friendly integer type (UCHAR) instead of uint8_t to
        // avoid pulling in the user-mode CRT headers.
        return static_cast<UCHAR>(lvl) <= static_cast<UCHAR>(g_MinLogLevel);
    }

    inline void PrintV(LogLevel lvl, const char* fmt, va_list args) {
        if (!ShouldLog(lvl))
            return;

        char buffer[512] = {};
        char final[512] = {};

        // Format main message
        NTSTATUS status = RtlStringCbVPrintfA(
            buffer,
            sizeof(buffer),
            fmt,
            args
        );

        if (!NT_SUCCESS(status)) {
            // truncated or failed, still continue
        }

        // Add prefix
        RtlStringCbPrintfA(
            final,
            sizeof(final),
            "%s%s\n",
            GetPrefix(lvl),
            buffer
        );

        DbgPrintEx(
            GetComponentId(),
            GetLevel(lvl),
            "%s",
            final
        );
    }

    inline void Print(LogLevel lvl, const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        PrintV(lvl, fmt, args);
        va_end(args);
    }

    inline void Info(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        PrintV(LogLevel::Info, fmt, args);
        va_end(args);
    }

    inline void Warn(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        PrintV(LogLevel::Warn, fmt, args);
        va_end(args);
    }

    inline void Error(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        PrintV(LogLevel::Error, fmt, args);
        va_end(args);
    }

    inline void Success(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        PrintV(LogLevel::Success, fmt, args);
        va_end(args);
    }

    inline void Debug(const char* fmt, ...) {
        va_list args; va_start(args, fmt);
        PrintV(LogLevel::Debug, fmt, args);
        va_end(args);
    }
}