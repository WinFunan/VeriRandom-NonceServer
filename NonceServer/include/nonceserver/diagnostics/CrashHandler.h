// CrashHandler.h: 进程级崩溃诊断（未处理 SEH 异常写 minidump）。
#pragma once

#include <string>

namespace nonceserver {

// 安装 SetUnhandledExceptionFilter + std::terminate 处理器。
// 返回 minidump 输出路径（位于 %TEMP%）。
std::string InstallCrashHandler();

}  // namespace nonceserver
