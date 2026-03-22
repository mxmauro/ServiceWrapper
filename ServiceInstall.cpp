#include <Windows.h>
#include <iostream>
#include <vector>
#include <exception>
#include "CommandLine.h"
#include "ScopedResource.h"
#include "ServiceInstall.h"
#include "StringUtils.h"
#include "TraceLogger.h"

// -----------------------------------------------------------------------------

inline void CloseServiceHandleDeleter(SC_HANDLE p) noexcept
{
    if (p)
    {
        ::CloseServiceHandle(p);
    }
}

using ScopedResourceServiceHandle = ScopedResource<SC_HANDLE, CloseServiceHandleDeleter>;

// -----------------------------------------------------------------------------

static DWORD ToServiceStartType(StartupType stStartupType);
static std::wstring BuildServiceCommandLine(const ParsedArguments& argsParsed);
static DWORD ConfigureFailureActions(SC_HANDLE hService, const RestartOptions& roRestartOptions);
static DWORD ConfigureDelayedAutoStart(SC_HANDLE hService, StartupType stStartupType);
static DWORD RegisterEventLogSource(const std::wstring& strServiceName);
static DWORD UnregisterEventLogSource(const std::wstring& strServiceName);

// -----------------------------------------------------------------------------

DWORD InstallService(const ParsedArguments &argsParsed)
{
    ScopedResourceServiceHandle cManagerHandleHolder;
    ScopedResourceServiceHandle cServiceHandleHolder;
    std::wstring strServiceCommandLine;
    const wchar_t *pszAccount = nullptr;
    const wchar_t *pszPassword = nullptr;
    DWORD dwOsError;

    if (argsParsed.strServiceName.empty() || argsParsed.strTargetBinaryPath.empty())
    {
        std::wcerr << L"Error: Missing service name and target binary path.\n";
        std::wcerr << BuildUsageText();
        return ERROR_INVALID_PARAMETER;
    }

    try
    {
        strServiceCommandLine = BuildServiceCommandLine(argsParsed);
    }
    catch (const std::bad_alloc &)
    {
        std::wcerr << L"Error: Setting up service parameters - " << GetLastErrorMessage(ERROR_OUTOFMEMORY) << L"\n";
        return ERROR_OUTOFMEMORY;
    }
    catch (...)
    {
        std::wcerr << L"Error: Setting up service parameters - " << GetLastErrorMessage(ERROR_UNHANDLED_ERROR) << L"\n";
        return ERROR_UNHANDLED_ERROR;
    }

    cManagerHandleHolder.reset(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!cManagerHandleHolder)
    {
        dwOsError = GetLastError();
        std::wcerr << L"Error: Unable to access to service manager - " << GetLastErrorMessage(dwOsError) << L"\n";
        return dwOsError;
    }

    pszAccount = StrNoCaseEqual(argsParsed.strAccount, L"LocalSystem") ? nullptr : argsParsed.strAccount.c_str();
    pszPassword = !pszAccount ? nullptr : (argsParsed.strPassword.empty() ? nullptr : argsParsed.strPassword.c_str());
    cServiceHandleHolder.reset(CreateServiceW(cManagerHandleHolder.get(), argsParsed.strServiceName.c_str(),
                                              argsParsed.strDisplayName.c_str(),
                                              SERVICE_CHANGE_CONFIG | SERVICE_START | DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS,
                                              SERVICE_WIN32_OWN_PROCESS, ToServiceStartType(argsParsed.startupType),
                                              SERVICE_ERROR_NORMAL, strServiceCommandLine.c_str(), nullptr, nullptr, nullptr,
                                              pszAccount, pszPassword));
    if (!cServiceHandleHolder)
    {
        dwOsError = GetLastError();
        std::wcerr << L"Error: Failed to create service - " << GetLastErrorMessage(dwOsError) << L"\n";
        return dwOsError;
    }

    dwOsError = ConfigureFailureActions(cServiceHandleHolder.get(), argsParsed.restartOptions);
    if (dwOsError == NOERROR)
    {
        dwOsError = ConfigureDelayedAutoStart(cServiceHandleHolder.get(), argsParsed.startupType);
    }
    if (dwOsError != NOERROR)
    {
        std::wcerr << L"Error: Failed to configure new service - " << GetLastErrorMessage(dwOsError) << L"\n";
        DeleteService(cServiceHandleHolder.get());
        return dwOsError;
    }

    dwOsError = RegisterEventLogSource(argsParsed.strServiceName);
    if (dwOsError != NOERROR)
    {
        std::wcerr << L"Error: Failed to register event log source - " << GetLastErrorMessage(dwOsError) << L"\n";
        DeleteService(cServiceHandleHolder.get());
        return dwOsError;
    }

    std::wcout << L"Service " << argsParsed.strServiceName << L" successfully installed.\n";
    return NOERROR;
}

DWORD UninstallService(const ParsedArguments &argsParsed)
{
    ScopedResourceServiceHandle cManagerHandleHolder;
    ScopedResourceServiceHandle cServiceHandleHolder;
    SERVICE_STATUS_PROCESS serviceStatusProcess{};
    DWORD dwBytesNeeded = 0;
    SERVICE_STATUS serviceStatus{};
    DWORD dwOsError;

    if (argsParsed.strServiceName.empty())
    {
        std::wcerr << BuildUsageText();
        return ERROR_INVALID_PARAMETER;
    }

    cManagerHandleHolder.reset(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!cManagerHandleHolder)
    {
        dwOsError = GetLastError();
        std::wcerr << L"Error: Unable to access to service manager - " << GetLastErrorMessage(dwOsError) << L"\n";
        return dwOsError;
    }

    cServiceHandleHolder.reset(OpenServiceW(cManagerHandleHolder.get(), argsParsed.strServiceName.c_str(),
                                            DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS));
    if (!cServiceHandleHolder)
    {
        dwOsError = GetLastError();
        std::wcerr << L"Error: Unable to access the service - " << GetLastErrorMessage(dwOsError) << L"\n";
        return dwOsError;
    }

    if (QueryServiceStatusEx(cServiceHandleHolder.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&serviceStatusProcess),
                             sizeof(serviceStatusProcess), &dwBytesNeeded) &&
        serviceStatusProcess.dwCurrentState != SERVICE_STOPPED)
    {
        ControlService(cServiceHandleHolder.get(), SERVICE_CONTROL_STOP, &serviceStatus);

        for (int nIndex = 0; nIndex < 20; ++nIndex)
        {
            Sleep(500);
            if (!QueryServiceStatusEx(cServiceHandleHolder.get(), SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&serviceStatusProcess),
                                      sizeof(serviceStatusProcess), &dwBytesNeeded))
            {
                break;
            }

            if (serviceStatusProcess.dwCurrentState == SERVICE_STOPPED)
            {
                break;
            }
        }
    }

    if (!DeleteService(cServiceHandleHolder.get()))
    {
        dwOsError = GetLastError();
        std::wcerr << L"Error: Failed to delete service - " << GetLastErrorMessage(dwOsError) << L"\n";
        return dwOsError;
    }

    dwOsError = UnregisterEventLogSource(argsParsed.strServiceName);
    if (dwOsError != NOERROR && dwOsError != ERROR_FILE_NOT_FOUND)
    {
        std::wcerr << L"Error: Failed to unregister event log source - " << GetLastErrorMessage(dwOsError) << L"\n";
        return dwOsError;
    }

    std::wcout << L"Service " << argsParsed.strServiceName << L" successfully uninstalled.\n";
    return NOERROR;
}

// -----------------------------------------------------------------------------

static DWORD ToServiceStartType(StartupType stStartupType)
{
    switch (stStartupType)
    {
    case StartupType::Manual:
        return SERVICE_DEMAND_START;

    case StartupType::Automatic:
    case StartupType::DelayedAutomatic:
        return SERVICE_AUTO_START;
    }

    return SERVICE_DEMAND_START;
}

static std::wstring BuildServiceCommandLine(const ParsedArguments& argsParsed)
{
    std::vector<std::wstring> vecParts = {
        GetCurrentModulePath(),
        L"--service",
        L"--name",
        argsParsed.strServiceName,
        L"--target",
        argsParsed.strTargetBinaryPath
    };

    if (!argsParsed.strTargetArguments.empty())
    {
        vecParts.push_back(L"--child-args");
        vecParts.push_back(argsParsed.strTargetArguments);
    }

    return JoinCommandLine(vecParts);
}

static DWORD ConfigureFailureActions(SC_HANDLE hService, const RestartOptions& roRestartOptions)
{
    std::vector<SC_ACTION> vecActions;
    SERVICE_FAILURE_ACTIONSW failureActions{};
    SERVICE_FAILURE_ACTIONS_FLAG failureActionFlag{};

    try
    {
        for (DWORD dwIndex = 0; dwIndex < roRestartOptions.dwRestartCount; ++dwIndex)
        {
            vecActions.push_back({ SC_ACTION_RESTART, roRestartOptions.dwRestartDelayMs });
        }
    }
    catch (std::bad_alloc&)
    {
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    catch (...)
    {
        return ERROR_UNHANDLED_EXCEPTION;
    }

    failureActions.dwResetPeriod = roRestartOptions.dwResetPeriodSeconds;
    failureActions.cActions = static_cast<DWORD>(vecActions.size());
    failureActions.lpsaActions = vecActions.empty() ? nullptr : vecActions.data();
    if (!ChangeServiceConfig2W(hService, SERVICE_CONFIG_FAILURE_ACTIONS, &failureActions))
    {
        return GetLastError();
    }

    failureActionFlag.fFailureActionsOnNonCrashFailures = FALSE;
    if (!ChangeServiceConfig2W(hService, SERVICE_CONFIG_FAILURE_ACTIONS_FLAG, &failureActionFlag))
    {
        return GetLastError();
    }

    return NOERROR;
}

static DWORD ConfigureDelayedAutoStart(SC_HANDLE hService, StartupType stStartupType)
{
    SERVICE_DELAYED_AUTO_START_INFO delayedAutoStartInfo{};

    delayedAutoStartInfo.fDelayedAutostart = stStartupType == StartupType::DelayedAutomatic;
    if (!ChangeServiceConfig2W(hService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayedAutoStartInfo))
    {
        return GetLastError();
    }

    return NOERROR;
}

static DWORD RegisterEventLogSource(const std::wstring& strServiceName)
{
    HKEY hEventSourceKey = nullptr;
    DWORD dwDisposition;
    std::wstring strMessageFilePath;
    std::wstring strKeyPath;
    LONG lResult;

    try
    {
        strMessageFilePath = GetCurrentModulePath();
        strKeyPath = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";
        strKeyPath.append(strServiceName);
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
    catch (...)
    {
        return ERROR_UNHANDLED_EXCEPTION;
    }

    dwDisposition = 0;
    lResult = RegCreateKeyExW(HKEY_LOCAL_MACHINE, strKeyPath.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr,
                              &hEventSourceKey, &dwDisposition);
    if (lResult == ERROR_SUCCESS)
    {
        lResult = RegSetValueExW(hEventSourceKey, L"EventMessageFile", 0, REG_SZ,
                                 reinterpret_cast<const BYTE *>(strMessageFilePath.c_str()),
                                 static_cast<DWORD>((strMessageFilePath.size() + 1) * sizeof(wchar_t)));
        if (lResult == ERROR_SUCCESS)
        {
            DWORD dwTypesSupported = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
            lResult = RegSetValueExW(hEventSourceKey, L"TypesSupported", 0, REG_DWORD, reinterpret_cast<const BYTE *>(&dwTypesSupported),
                                     sizeof(dwTypesSupported));
        }

        RegCloseKey(hEventSourceKey);
    }

    if (lResult != ERROR_SUCCESS)
    {
        UnregisterEventLogSource(strServiceName);
    }
    return static_cast<DWORD>(lResult);
}

static DWORD UnregisterEventLogSource(const std::wstring& strServiceName)
{
    std::wstring strKeyPath;

    try
    {
        strKeyPath = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\";
        strKeyPath.append(strServiceName);
    }
    catch (const std::bad_alloc&)
    {
        return ERROR_OUTOFMEMORY;
    }
    catch (...)
    {
        return ERROR_UNHANDLED_EXCEPTION;
    }

    return static_cast<DWORD>(RegDeleteTreeW(HKEY_LOCAL_MACHINE, strKeyPath.c_str()));
}
