#include "Utils.hpp"

#include <cstdarg>
#include <cwchar>
#include <mutex>

double Clock::seconds() {
    static const LARGE_INTEGER freq = [] { LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)freq.QuadPart;
}

std::wstring Paths::exeDir() {
    wchar_t path[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"";
    std::wstring s(path, n);
    const size_t k = s.find_last_of(L'\\');
    return k == std::wstring::npos ? L"" : s.substr(0, k + 1);
}

std::string Paths::toUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], len, nullptr, nullptr);
    return s;
}

void Logger::write(const wchar_t* fmt, ...) {
    static std::mutex logMutex;
    wchar_t text[512];
    va_list ap;
    va_start(ap, fmt);
    if (vswprintf(text, 512, fmt, ap) < 0) text[0] = 0;
    va_end(ap);
    text[511] = 0;

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[600];
    swprintf(line, 600, L"%04u-%02u-%02u %02u:%02u:%02u  %ls\r\n",
             t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, text);
    char utf8[1800];
    const int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, (int)sizeof(utf8), nullptr, nullptr);
    if (n <= 1) return;

    const std::wstring dir = Paths::exeDir();
    if (dir.empty()) return;
    const std::wstring p = dir + L"HandBoard.log";

    std::lock_guard<std::mutex> lk(logMutex);
    HANDLE f = CreateFileW(p.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size;
    if (GetFileSizeEx(f, &size) && size.QuadPart > 1024 * 1024) {      // don't let it grow forever
        CloseHandle(f);
        f = CreateFileW(p.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
    }
    DWORD w = 0;
    WriteFile(f, utf8, (DWORD)(n - 1), &w, nullptr);
    CloseHandle(f);
}
