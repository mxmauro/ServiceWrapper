#include <stdexcept>
#include "CommandLine.h"

// -----------------------------------------------------------------------------

static std::wstring RequireValue(int nArgc, wchar_t* ppszArgv[], int& nIndex);
static DWORD ParseUnsignedValue(const std::wstring& strValue);

// -----------------------------------------------------------------------------

ParsedArguments ParseArguments(int nArgc, wchar_t* ppszArgv[])
{
    ParsedArguments argsParsed;
    std::wstring strStartup;

    if (nArgc <= 1)
    {
        return argsParsed;
    }

    if (_wcsicmp(ppszArgv[1], L"install") == 0)
    {
        argsParsed.appMode = AppMode::Install;
    }
    else if (_wcsicmp(ppszArgv[1], L"uninstall") == 0)
    {
        argsParsed.appMode = AppMode::Uninstall;
    }
    else if (_wcsicmp(ppszArgv[1], L"--service") == 0)
    {
        argsParsed.appMode = AppMode::Service;
    }
    else
    {
        throw std::runtime_error("Invalid application mode");
    }

    for (int nIndex = 2; nIndex < nArgc; ++nIndex)
    {
        if (_wcsicmp(ppszArgv[nIndex], L"--name") == 0)
        {
            argsParsed.strServiceName = RequireValue(nArgc, ppszArgv, nIndex);
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--display-name") == 0)
        {
            argsParsed.strDisplayName = RequireValue(nArgc, ppszArgv, nIndex);
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--target") == 0)
        {
            argsParsed.strTargetBinaryPath = RequireValue(nArgc, ppszArgv, nIndex);
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--child-args") == 0)
        {
            argsParsed.strTargetArguments = RequireValue(nArgc, ppszArgv, nIndex);
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--account") == 0)
        {
            argsParsed.strAccount = RequireValue(nArgc, ppszArgv, nIndex);
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--password") == 0)
        {
            argsParsed.strPassword = RequireValue(nArgc, ppszArgv, nIndex);
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--startup") == 0)
        {
            strStartup = RequireValue(nArgc, ppszArgv, nIndex);
            if (_wcsicmp(strStartup.c_str(), L"manual") == 0)
            {
                argsParsed.startupType = StartupType::Manual;
            }
            else if (_wcsicmp(strStartup.c_str(), L"auto") == 0)
            {
                argsParsed.startupType = StartupType::Automatic;
            }
            else if (_wcsicmp(strStartup.c_str(), L"delayed-auto") == 0)
            {
                argsParsed.startupType = StartupType::DelayedAutomatic;
            }
            else
            {
                throw std::runtime_error("Invalid startup type");
            }
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--restart-count") == 0)
        {
            argsParsed.restartOptions.dwRestartCount = ParseUnsignedValue(RequireValue(nArgc, ppszArgv, nIndex));
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--restart-delay-ms") == 0)
        {
            argsParsed.restartOptions.dwRestartDelayMs = ParseUnsignedValue(RequireValue(nArgc, ppszArgv, nIndex));
        }
        else if (_wcsicmp(ppszArgv[nIndex], L"--restart-reset-seconds") == 0)
        {
            argsParsed.restartOptions.dwResetPeriodSeconds = ParseUnsignedValue(RequireValue(nArgc, ppszArgv, nIndex));
        }
        else
        {
            throw std::runtime_error("Unknown command line option");
        }
    }

    if (argsParsed.strDisplayName.empty())
    {
        argsParsed.strDisplayName = argsParsed.strServiceName;
    }

    return argsParsed;
}

std::wstring BuildUsageText()
{
    return L"Usage:\n"
           L"  sw install --name <service-name> --target <child-exe> [options]\n"
           L"  sw uninstall --name <service-name>\n"
           L"\n"
           L"Install options:\n"
           L"  --child-args <raw arguments>\n"
           L"  --display-name <display name>\n"
           L"  --account <user account>           defaults to LocalSystem\n"
           L"  --password <password>              required for most custom users\n"
           L"  --startup manual|auto|delayed-auto defaults to manual\n"
           L"  --restart-count <n>                defaults to 3\n"
           L"  --restart-delay-ms <ms>            defaults to 5000\n"
           L"  --restart-reset-seconds <s>        defaults to 86400\n";
}

// -----------------------------------------------------------------------------

static std::wstring RequireValue(int nArgc, wchar_t* ppszArgv[], int& nIndex)
{
    if (nIndex + 1 >= nArgc)
    {
        throw std::runtime_error("Missing command line value");
    }

    ++nIndex;
    return ppszArgv[nIndex];
}

static DWORD ParseUnsignedValue(const std::wstring& strValue)
{
    size_t nConsumed = 0;
    unsigned long ulParsed = 0;

    ulParsed = std::stoul(strValue, &nConsumed, 10);
    if (nConsumed != strValue.size())
    {
        throw std::runtime_error("Invalid command line numeric value");
    }

    return static_cast<DWORD>(ulParsed);
}
