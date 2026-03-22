#include <Windows.h>
#include <exception>
#include <ServiceWrapperEvents.h>
#include "StringUtils.h"
#include "TraceLogger.h"

// -----------------------------------------------------------------------------

static std::wstring g_strServiceName;
static std::wstring g_strBinaryPath;
static HANDLE g_hEventSource = nullptr;

// -----------------------------------------------------------------------------

static std::wstring BuildErrorMessage(DWORD dwWin32Error) noexcept;
static std::wstring ToDecimalString(DWORD dwValue);

// -----------------------------------------------------------------------------

bool InitializeTraceLogger(const std::wstring& strServiceName, const std::wstring& strBinaryPath) noexcept
{
    if (g_hEventSource)
    {
        return true;
    }

    try
    {
        g_strServiceName = strServiceName;
        g_strBinaryPath = strBinaryPath;
    }
    catch (std::exception&)
    {
        return false;
    }

    g_hEventSource = RegisterEventSourceW(nullptr, g_strServiceName.c_str());
    if (!g_hEventSource)
    {
        g_strServiceName.clear();
        g_strBinaryPath.clear();
        return false;
    }

    return true;
}

void TerminateTraceLogger() noexcept
{
    if (g_hEventSource)
    {
        DeregisterEventSource(g_hEventSource);
        g_hEventSource = nullptr;
    }

    g_strServiceName.clear();
    g_strBinaryPath.clear();
}

void LogStartSetupFailed(DWORD dwWin32Error) noexcept
{
    std::wstring strErrorCode;
    std::wstring strErrorMessage;
    LPCWSTR rgszStrings[4];

    if (!g_hEventSource)
    {
        return;
    }

    strErrorCode = ToDecimalString(dwWin32Error);
    strErrorMessage = BuildErrorMessage(dwWin32Error);
    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    rgszStrings[2] = strErrorCode.c_str();
    rgszStrings[3] = strErrorMessage.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_ERROR_TYPE, 0, MSG_START_SETUP_FAILED, nullptr, 4, 0, rgszStrings, nullptr);
}

void LogStartFailed(DWORD dwWin32Error) noexcept
{
    std::wstring strErrorCode;
    std::wstring strErrorMessage;
    LPCWSTR rgszStrings[4];

    if (!g_hEventSource)
    {
        return;
    }

    strErrorCode = ToDecimalString(dwWin32Error);
    strErrorMessage = BuildErrorMessage(dwWin32Error);
    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    rgszStrings[2] = strErrorCode.c_str();
    rgszStrings[3] = strErrorMessage.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_ERROR_TYPE, 0, MSG_START_FAILED, nullptr, 4, 0, rgszStrings, nullptr);
}

void LogStarted(DWORD dwProcessId) noexcept
{
    std::wstring strProcessId;
    LPCWSTR rgszStrings[3];

    if (!g_hEventSource)
    {
        return;
    }

    strProcessId = ToDecimalString(dwProcessId);
    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    rgszStrings[2] = strProcessId.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_INFORMATION_TYPE, 0, MSG_STARTED, nullptr, 3, 0, rgszStrings, nullptr);
}

void LogStopSkipped() noexcept
{
    LPCWSTR rgszStrings[2];

    if (!g_hEventSource)
    {
        return;
    }

    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_INFORMATION_TYPE, 0, MSG_STOP_SKIPPED, nullptr, 2, 0, rgszStrings, nullptr);
}

void LogTerminateFailed(DWORD dwProcessId, DWORD dwWin32Error) noexcept
{
    std::wstring strProcessId;
    std::wstring strErrorCode;
    std::wstring strErrorMessage;
    LPCWSTR rgszStrings[5];

    if (!g_hEventSource)
    {
        return;
    }

    strProcessId = ToDecimalString(dwProcessId);
    strErrorCode = ToDecimalString(dwWin32Error);
    strErrorMessage = BuildErrorMessage(dwWin32Error);
    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    rgszStrings[2] = strProcessId.c_str();
    rgszStrings[3] = strErrorCode.c_str();
    rgszStrings[4] = strErrorMessage.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_ERROR_TYPE, 0, MSG_TERMINATE_FAILED, nullptr, 5, 0, rgszStrings, nullptr);
}

void LogTerminatedForcefully(DWORD dwProcessId, DWORD dwExitCode) noexcept
{
    std::wstring strProcessId;
    std::wstring strExitCode;
    LPCWSTR rgszStrings[4];

    if (!g_hEventSource)
    {
        return;
    }

    strProcessId = ToDecimalString(dwProcessId);
    strExitCode = ToDecimalString(dwExitCode);
    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    rgszStrings[2] = strProcessId.c_str();
    rgszStrings[3] = strExitCode.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_WARNING_TYPE, 0, MSG_TERMINATED_FORCEFULLY, nullptr, 4, 0, rgszStrings, nullptr);
}

void LogStoppedGracefully(DWORD dwProcessId, DWORD dwExitCode) noexcept
{
    std::wstring strProcessId;
    std::wstring strExitCode;
    LPCWSTR rgszStrings[4];

    if (!g_hEventSource)
    {
        return;
    }

    strProcessId = ToDecimalString(dwProcessId);
    strExitCode = ToDecimalString(dwExitCode);
    rgszStrings[0] = g_strServiceName.c_str();
    rgszStrings[1] = g_strBinaryPath.c_str();
    rgszStrings[2] = strProcessId.c_str();
    rgszStrings[3] = strExitCode.c_str();
    ReportEventW(g_hEventSource, EVENTLOG_INFORMATION_TYPE, 0, MSG_STOPPED_GRACEFULLY, nullptr, 4, 0, rgszStrings, nullptr);
}

// -----------------------------------------------------------------------------

static std::wstring BuildErrorMessage(DWORD dwWin32Error) noexcept
{
    try
    {
        return GetLastErrorMessage(dwWin32Error);
    }
    catch (std::exception&)
    {
        return {};
    }
}

static std::wstring ToDecimalString(DWORD dwValue)
{
    return std::to_wstring(dwValue);
}
