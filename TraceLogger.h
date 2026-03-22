#pragma once

#include <Windows.h>
#include <string>

// -----------------------------------------------------------------------------

bool InitializeTraceLogger(const std::wstring& strServiceName, const std::wstring& strBinaryPath) noexcept;
void TerminateTraceLogger() noexcept;

void LogStartSetupFailed(DWORD dwWin32Error) noexcept;
void LogStartFailed(DWORD dwWin32Error) noexcept;
void LogStarted(DWORD dwProcessId) noexcept;
void LogStopSkipped() noexcept;
void LogTerminateFailed(DWORD dwProcessId, DWORD dwWin32Error) noexcept;
void LogTerminatedForcefully(DWORD dwProcessId, DWORD dwExitCode) noexcept;
void LogStoppedGracefully(DWORD dwProcessId, DWORD dwExitCode) noexcept;
