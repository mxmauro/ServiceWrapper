#include <Windows.h>
#include <iostream>
#include <stdexcept>
#include "CommandLine.h"
#include "ServiceHost.h"
#include "ServiceInstall.h"
#include "StringUtils.h"

// -----------------------------------------------------------------------------

int wmain(int nArgc, wchar_t* ppszArgv[])
{
    ParsedArguments parsedArgs;
    DWORD dwServiceResult;

    try
    {
        parsedArgs = ParseArguments(nArgc, ppszArgv);
    }
    catch (const std::bad_alloc&)
    {
        std::wcerr << L"Error: Out of memory while parsing arguments.\n";
        return ERROR_OUTOFMEMORY;
    }
    catch (const std::runtime_error& exError)
    {
        std::cerr << "Error: " << exError.what() << "\n";
        std::wcerr << BuildUsageText();
        return ERROR_INVALID_PARAMETER;
    }

    if (parsedArgs.appMode == AppMode::None)
    {
        std::wcout << BuildUsageText();
        return 0;
    }

    dwServiceResult = RunAsService(parsedArgs);
    if (dwServiceResult == 0)
    {
        return 0;
    }

    if (dwServiceResult != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
    {
        if (parsedArgs.appMode == AppMode::Service)
        {
            std::wcerr << L"Error: Service control dispatcher start failed: " << GetLastErrorMessage(dwServiceResult) << L"\n";
            return static_cast<int>(dwServiceResult);
        }
    }

    switch (parsedArgs.appMode)
    {
        case AppMode::Install:
            dwServiceResult = InstallService(parsedArgs);
            return static_cast<int>(dwServiceResult);

        case AppMode::Uninstall:
            dwServiceResult = UninstallService(parsedArgs);
            return static_cast<int>(dwServiceResult);
    }

    std::wcerr << L"Error: This command line is intended to be started by the Service Control Manager.\n";
    return ERROR_INVALID_PARAMETER;
}
