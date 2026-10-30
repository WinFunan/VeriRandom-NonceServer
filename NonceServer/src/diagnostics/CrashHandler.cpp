#include "nonceserver/diagnostics/CrashHandler.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

namespace nonceserver {

namespace {

std::wstring g_dumpPath;

std::wstring DefaultDumpPath() {
    wchar_t tempPath[MAX_PATH] = {0};
    const DWORD length = GetTempPathW(MAX_PATH, tempPath);
    std::wstring path = length > 0 ? std::wstring(tempPath, length) : std::wstring(L".\\");
    path += L"VeriRandom-NonceServer-crash.dmp";
    return path;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length,
                        nullptr, nullptr);
    return utf8;
}

void WriteDump(EXCEPTION_POINTERS* exceptionPointers) {
    HANDLE file = CreateFileW(g_dumpPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION information{};
        information.ThreadId = GetCurrentThreadId();
        information.ExceptionPointers = exceptionPointers;
        information.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                          static_cast<MINIDUMP_TYPE>(MiniDumpWithThreadInfo | MiniDumpWithHandleData),
                          exceptionPointers ? &information : nullptr, nullptr, nullptr);
        CloseHandle(file);
    }
}

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* exceptionPointers) {
    DWORD code = 0;
    void* address = nullptr;
    if (exceptionPointers && exceptionPointers->ExceptionRecord) {
        code = exceptionPointers->ExceptionRecord->ExceptionCode;
        address = exceptionPointers->ExceptionRecord->ExceptionAddress;
    }

    WriteDump(exceptionPointers);

    char message[512];
    std::snprintf(message, sizeof(message),
                  "[FATAL] unhandled exception code=0x%08lX address=%p dump=%ls\n", code, address,
                  g_dumpPath.c_str());
    std::fputs(message, stderr);
    std::fflush(stderr);
    OutputDebugStringA(message);
    return EXCEPTION_EXECUTE_HANDLER;
}

void TerminateHandler() {
    std::fputs("[FATAL] std::terminate (unhandled C++ exception)\n", stderr);
    std::fflush(stderr);
    // 转成 SEH 异常，以便统一走 CrashFilter 落 dump。
    RaiseException(0xE0000001UL, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    std::abort();
}

}  // namespace

std::string InstallCrashHandler() {
    g_dumpPath = DefaultDumpPath();
    SetUnhandledExceptionFilter(CrashFilter);
    std::set_terminate(TerminateHandler);
    return WideToUtf8(g_dumpPath);
}

}  // namespace nonceserver
