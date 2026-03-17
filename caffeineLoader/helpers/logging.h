#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <string_view>
#include <string>

namespace Caffeine::Logging {
    enum class LogLevel : std::uint8_t {
        Info,
        Warn,
        Error,
        Success,
        Debug
    };

    // helper to get console handle on-demand
    inline HANDLE GetConsoleHandle() { return GetStdHandle(STD_OUTPUT_HANDLE); }

    inline WORD GetDefaultColor() {
        CONSOLE_SCREEN_BUFFER_INFO info{};
        HANDLE h = GetConsoleHandle();
        if (h != nullptr && h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &info)) {
            return info.wAttributes;
        }
        return static_cast<WORD>(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    inline const WORD DefaultColor = GetDefaultColor();

    inline WORD GetColour(LogLevel lvl) {
        switch (lvl) {
        case LogLevel::Info:    return FOREGROUND_RED | FOREGROUND_BLUE;
        case LogLevel::Warn:    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case LogLevel::Error:   return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case LogLevel::Success: return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case LogLevel::Debug:   return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        }
        return DefaultColor;
    }

    inline const char* GetPrefix(const LogLevel lvl) {
        switch (lvl) {
        case LogLevel::Info:    return "[*] ";
        case LogLevel::Warn:    return "[!] ";
        case LogLevel::Error:   return "[-] ";
        case LogLevel::Success: return "[+] ";
        case LogLevel::Debug:   return "[D] ";
        }
        return "";
    }

    inline void PrintV(LogLevel lvl, const char* fmt, va_list args) {
        HANDLE h = GetConsoleHandle();
        WORD colour = GetColour(lvl);
        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            (void)SetConsoleTextAttribute(h, colour);
        }

        (void)std::fputs(GetPrefix(lvl), stdout);
        std::vprintf(fmt, args);
        (void)std::putchar('\n');
        (void)std::fflush(stdout);

        if (h != nullptr && h != INVALID_HANDLE_VALUE) {
            (void)SetConsoleTextAttribute(h, DefaultColor);
        }
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

    inline void warn(const char* fmt, ...) {
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

    inline void WaitForInteract(std::string_view prompt = "Press any key to continue...") {
        std::string s(prompt);
        Print(LogLevel::Info, "%s", s.c_str());
        std::getchar();
    }
}
