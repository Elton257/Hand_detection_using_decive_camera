// ============================================================================
//  Utils.hpp - small helpers: high-resolution clock, exe folder, log file
// ============================================================================
#pragma once
#include "WinCommon.hpp"

#include <string>

// Monotonic clock in seconds (QueryPerformanceCounter).
class Clock {
public:
    static double seconds();
};

// Paths and text conversion.
class Paths {
public:
    // Folder that contains HandBoard.exe, with a trailing backslash ("" on error).
    static std::wstring exeDir();
    // UTF-16 (Windows) -> UTF-8 (OpenCV file paths).
    static std::string toUtf8(const std::wstring& w);
};

// Appends timestamped lines to HandBoard.log next to the exe.
// Used to find out why the program closed. The file starts over once it exceeds 1 MB.
// Thread-safe.
class Logger {
public:
    static void write(const wchar_t* fmt, ...);
};
