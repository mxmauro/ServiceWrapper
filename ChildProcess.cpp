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
    CloseHandles();
}

DWORD ChildProcess::Start(const std::wstring& strBinaryPath, const std::wstring& strArguments) noexcept
{
    std::vector<wchar_t> vecCommandLine;
    STARTUPINFOW startupInfo{};
    std::wstring strCommandLine;
    std::wstring strWorkingDirectory;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION sJobObjExtLimitInfo;
    DWORD dwOsErr;

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

    m_hJob = CreateJobObjectW(nullptr, nullptr);
    if (!m_hJob)
    {
on_start_error:
        dwOsErr = GetLastError();
on_start_error2:
        CloseHandles();
        LogStartFailed(dwOsErr);
        return dwOsErr;
    }
    memset(&sJobObjExtLimitInfo, 0, sizeof(sJobObjExtLimitInfo));
    sJobObjExtLimitInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(m_hJob, JobObjectExtendedLimitInformation, &sJobObjExtLimitInfo, (DWORD)sizeof(sJobObjExtLimitInfo)))
    {
        goto on_start_error;
    }
    if (!CreateProcessW(strBinaryPath.c_str(), vecCommandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED,
                        nullptr, strWorkingDirectory.empty() ? nullptr : strWorkingDirectory.c_str(), &startupInfo, &m_piProcessInfo))
    {
        goto on_start_error;
    }
    if (!AssignProcessToJobObject(m_hJob, m_piProcessInfo.hProcess))
    {
        dwOsErr = GetLastError();
        TerminateProcess(m_piProcessInfo.hProcess, ERROR_PROCESS_ABORTED);
        goto on_start_error2;
    }
    if (ResumeThread(m_piProcessInfo.hThread) == (DWORD)-1)
    {
        dwOsErr = GetLastError();
        TerminateProcess(m_piProcessInfo.hProcess, ERROR_PROCESS_ABORTED);
        goto on_start_error2;
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
not_running:
        dwExitCode = ExitCode();
        TerminateJobObject(m_hJob, dwExitCode);
        LogStoppedGracefully(m_piProcessInfo.dwProcessId, dwExitCode);
        return;
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

    if (!IsRunning())
    {
        goto not_running;
    }

    if (TerminateJobObject(m_hJob, ERROR_PROCESS_ABORTED) || TerminateProcess(m_piProcessInfo.hProcess, ERROR_PROCESS_ABORTED))
    {
        WaitForExit(kdwChildStopTimeoutMs);
        dwExitCode = ERROR_PROCESS_ABORTED;
        LogTerminatedForcefully(m_piProcessInfo.dwProcessId, ERROR_PROCESS_ABORTED);

    }
    else
    {
        dwExitCode = ExitCode();
        LogTerminateFailed(m_piProcessInfo.dwProcessId, GetLastError());
    }
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
    DWORD dwExitCode = ERROR_SUCCESS;

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

void ChildProcess::CloseHandles() noexcept
{
    if (m_piProcessInfo.hThread)
    {
        CloseHandle(m_piProcessInfo.hThread);
    }
    if (m_piProcessInfo.hProcess)
    {
        CloseHandle(m_piProcessInfo.hProcess);
    }
    if (m_hJob)
    {
        CloseHandle(m_hJob);
    }
    memset(&m_piProcessInfo, 0, sizeof(m_piProcessInfo));
    m_hJob = nullptr;
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
