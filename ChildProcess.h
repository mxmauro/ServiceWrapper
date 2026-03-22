#pragma once

#include <Windows.h>
#include <string>

// -----------------------------------------------------------------------------

class ChildProcess
{
public:
    ChildProcess() = default;
    ChildProcess(const ChildProcess &) = delete;
    ChildProcess(ChildProcess &&) = delete;
    ~ChildProcess() noexcept;

    ChildProcess &operator= (const ChildProcess &) = delete;
    ChildProcess &operator= (ChildProcess &&) = delete;

    DWORD Start(const std::wstring& strBinaryPath, const std::wstring& strArguments) noexcept;

    DWORD WaitForExitOrStop(HANDLE hStopEvent) noexcept;
    void RequestStop() noexcept;
    DWORD WaitForExit(DWORD dwTimeoutMs) const noexcept;

    DWORD ProcessId() const noexcept;
    DWORD ExitCode() const noexcept;
    bool IsRunning() const noexcept;

private:
    static const DWORD kdwChildStopTimeoutMs = 10000;

    PROCESS_INFORMATION m_piProcessInfo{};
};
