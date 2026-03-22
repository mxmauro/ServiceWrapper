#include <Windows.h>
#include <new>
#include <exception>
#include "ServiceHost.h"
#include "ChildProcess.h"
#include "TraceLogger.h"
// -----------------------------------------------------------------------------

static SERVICE_STATUS_HANDLE g_hServiceStatus = nullptr;
static SERVICE_STATUS g_serviceStatus{};
static HANDLE g_hStopEvent = nullptr;
static const ParsedArguments* g_pArgsService = nullptr;

// -----------------------------------------------------------------------------

static void UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

static DWORD WINAPI ServiceControlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
static void WINAPI ServiceMain(DWORD dwArgc, LPWSTR* ppszArgv);

// -----------------------------------------------------------------------------

DWORD RunAsService(const ParsedArguments& argsParsed)
{
    SERVICE_TABLE_ENTRYW rgDispatchTable[] = {
        { const_cast<LPWSTR>(argsParsed.strServiceName.c_str()), ServiceMain },
        { nullptr, nullptr }
    };

    g_pArgsService = &argsParsed;

    return (StartServiceCtrlDispatcherW(rgDispatchTable)) ? NOERROR : GetLastError();
}

// -----------------------------------------------------------------------------

static void UpdateServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwCurrentState = dwCurrentState;
    g_serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_serviceStatus.dwWaitHint = dwWaitHint;
    g_serviceStatus.dwControlsAccepted = dwCurrentState == SERVICE_START_PENDING ? 0 : (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
    g_serviceStatus.dwCheckPoint = (dwCurrentState == SERVICE_RUNNING || dwCurrentState == SERVICE_STOPPED)
                                   ? 0 : g_serviceStatus.dwCheckPoint + 1;

    SetServiceStatus(g_hServiceStatus, &g_serviceStatus);
}

static DWORD WINAPI ServiceControlHandlerEx(DWORD dwControl, DWORD, LPVOID, LPVOID)
{
    switch (dwControl)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            UpdateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 10000);
            SetEvent(g_hStopEvent);
            return NO_ERROR;

        case SERVICE_CONTROL_INTERROGATE:
            SetServiceStatus(g_hServiceStatus, &g_serviceStatus);
            return NO_ERROR;
    }

    return ERROR_CALL_NOT_IMPLEMENTED;
}

static void WINAPI ServiceMain(DWORD, LPWSTR*)
{
    ChildProcess cChildProcess;
    bool bTraceLoggerInitialized = false;
    DWORD dwOsErr = 0;

    g_hServiceStatus = RegisterServiceCtrlHandlerExW(g_pArgsService->strServiceName.c_str(), ServiceControlHandlerEx, nullptr);
    if (!g_hServiceStatus)
    {
        return;
    }

    g_hStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_hStopEvent)
    {
        UpdateServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR, 10000);

    if (g_pArgsService->strTargetBinaryPath.empty())
    {
        dwOsErr = ERROR_INVALID_PARAMETER;
        goto cleanup;
    }

    bTraceLoggerInitialized = InitializeTraceLogger(g_pArgsService->strServiceName, g_pArgsService->strTargetBinaryPath);

    dwOsErr = cChildProcess.Start(g_pArgsService->strTargetBinaryPath, g_pArgsService->strTargetArguments);
    if (dwOsErr != NO_ERROR)
    {
        goto cleanup;
    }

    UpdateServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    if (cChildProcess.WaitForExitOrStop(g_hStopEvent) == WAIT_OBJECT_0 + 1)
    {
        cChildProcess.RequestStop();
        dwOsErr = NOERROR;
    }
    else
    {
        dwOsErr = cChildProcess.ExitCode();
        LogStoppedGracefully(cChildProcess.ProcessId(), dwOsErr);
    }

    UpdateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 2000);

cleanup:
    if (bTraceLoggerInitialized)
    {
        TerminateTraceLogger();
    }

    if (g_hStopEvent)
    {
        CloseHandle(g_hStopEvent);
        g_hStopEvent = nullptr;
    }

    UpdateServiceStatus(SERVICE_STOPPED, dwOsErr, 0);
}
