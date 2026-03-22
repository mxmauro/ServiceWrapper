#include <Windows.h>
#include <vector>
#include <exception>
#include "ChildProcess.h"
#include "StringUtils.h"
#include "TraceLogger.h"

// -----------------------------------------------------------------------------

typedef struct _FIND_MAIN_WINDOW_DATA {
    DWORD dwProcessId;
    HWND hMainWnd;
} FIND_MAIN_WINDOW_DATA, *LPFIND_MAIN_WINDOW_DATA;

// -----------------------------------------------------------------------------

static HWND FindMainWindow(DWORD dwPid);
static BOOL CALLBACK EWP_FindMainWindow(_In_ HWND hwnd, _In_ LPARAM lParam);

// -----------------------------------------------------------------------------

ChildProcess::~ChildProcess() noexcept
{
    if (m_piProcessInfo.hThread)
    {
        CloseHandle(m_piProcessInfo.hThread);
    }

    if (m_piProcessInfo.hProcess)
    {
        CloseHandle(m_piProcessInfo.hProcess);
    }
}

DWORD ChildProcess::Start(const std::wstring& strBinaryPath, const std::wstring& strArguments) noexcept
{
    std::vector<wchar_t> vecCommandLine;
    STARTUPINFOW startupInfo{};
    std::wstring strCommandLine;
    std::wstring strWorkingDirectory;

    try
    {
        strCommandLine = QuoteForCommandLine(strBinaryPath);
        startupInfo.cb = sizeof(startupInfo);
        strWorkingDirectory = GetDirectoryFromPath(strBinaryPath);

        if (!strArguments.empty())
        {
            strCommandLine.push_back(L' ');
            strCommandLine.append(strArguments);
        }

        vecCommandLine.assign(strCommandLine.begin(), strCommandLine.end());
        vecCommandLine.push_back(L'\0');
    }
    catch (const std::bad_alloc&)
    {
        LogStartSetupFailed(ERROR_OUTOFMEMORY);
        return ERROR_OUTOFMEMORY;
    }
    catch (...)
    {
        LogStartSetupFailed(ERROR_UNHANDLED_EXCEPTION);
        return ERROR_UNHANDLED_EXCEPTION;
    }

    if (!CreateProcessW(strBinaryPath.c_str(), vecCommandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP, nullptr,
                        strWorkingDirectory.empty() ? nullptr : strWorkingDirectory.c_str(), &startupInfo, &m_piProcessInfo))
    {
        DWORD dwOsErr = GetLastError();
        LogStartFailed(dwOsErr);
        return dwOsErr;
    }

    LogStarted(m_piProcessInfo.dwProcessId);
    return NOERROR;
}

void ChildProcess::RequestStop() noexcept
{
    DWORD dwAttachConsoleError = NO_ERROR;
    DWORD dwExitCode = STILL_ACTIVE;
    BOOL bSent;

    if (!IsRunning())
    {
        goto graceful_stop;
    }

    bSent = FALSE;
    if (AttachConsole(m_piProcessInfo.dwProcessId))
    {
        bSent = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, m_piProcessInfo.dwProcessId);
        FreeConsole();
    }
    else
    {
        dwAttachConsoleError = GetLastError();
    }

    if (!bSent)
    {
        HWND hwnd = FindMainWindow(m_piProcessInfo.dwProcessId);
        if (hwnd && (GetWindowLongW(hwnd, GWL_STYLE) & WS_DISABLED) == 0)
        {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            bSent = TRUE;
        }
    }

    if (bSent)
    {
        WaitForExit(kdwChildStopTimeoutMs);
    }

    if (IsRunning())
    {
        if (TerminateProcess(m_piProcessInfo.hProcess, ERROR_PROCESS_ABORTED))
        {
            WaitForExit(kdwChildStopTimeoutMs);
            dwExitCode = ExitCode();
            LogTerminatedForcefully(m_piProcessInfo.dwProcessId, dwExitCode);

        }
        else
        {
            LogTerminateFailed(m_piProcessInfo.dwProcessId, GetLastError());
        }
        return;
    }

graceful_stop:
    dwExitCode = ExitCode();
    LogStoppedGracefully(m_piProcessInfo.dwProcessId, dwExitCode);
}

DWORD ChildProcess::WaitForExitOrStop(HANDLE hStopEvent) noexcept
{
    HANDLE rgHandles[2] = { m_piProcessInfo.hProcess, hStopEvent };
    return WaitForMultipleObjects(2, rgHandles, FALSE, INFINITE);
}

DWORD ChildProcess::WaitForExit(DWORD dwTimeoutMs) const noexcept
{
    if (!m_piProcessInfo.hProcess)
    {
        return WAIT_OBJECT_0;
    }

    return WaitForSingleObject(m_piProcessInfo.hProcess, dwTimeoutMs);
}

DWORD ChildProcess::ProcessId() const noexcept
{
    return m_piProcessInfo.dwProcessId;
}

DWORD ChildProcess::ExitCode() const noexcept
{
    DWORD dwExitCode = 0;

    if (m_piProcessInfo.hProcess)
    {
        GetExitCodeProcess(m_piProcessInfo.hProcess, &dwExitCode);
    }

    return dwExitCode;
}

bool ChildProcess::IsRunning() const noexcept
{
    if (!m_piProcessInfo.hProcess)
    {
        return false;
    }

    return WaitForSingleObject(m_piProcessInfo.hProcess, 0) == WAIT_TIMEOUT;
}

// -----------------------------------------------------------------------------

static HWND FindMainWindow(DWORD dwPid)
{
    FIND_MAIN_WINDOW_DATA sData;

    sData.dwProcessId = dwPid;
    sData.hMainWnd = nullptr;
    EnumWindows(&EWP_FindMainWindow, reinterpret_cast<LPARAM>(&sData));
    return sData.hMainWnd;
}

static BOOL CALLBACK EWP_FindMainWindow(_In_ HWND hwnd, _In_ LPARAM lParam)
{
    LPFIND_MAIN_WINDOW_DATA lpData = reinterpret_cast<LPFIND_MAIN_WINDOW_DATA>(lParam);
    DWORD dwPid;

    GetWindowThreadProcessId(hwnd, &dwPid);

    if (dwPid == lpData->dwProcessId && GetWindow(hwnd, GW_OWNER) == nullptr && IsWindowVisible(hwnd))
    {
        lpData->hMainWnd = hwnd;
        return FALSE;
    }
    return TRUE;
}
