#pragma once

#include <Windows.h>
#include <string>

// -----------------------------------------------------------------------------

enum class AppMode
{
    None,
    Install,
    Uninstall,
    Service
};

enum class StartupType
{
    Manual,
    Automatic,
    DelayedAutomatic
};

// -----------------------------------------------------------------------------

struct RestartOptions
{
    DWORD dwRestartCount = 3;
    DWORD dwRestartDelayMs = 5000;
    DWORD dwResetPeriodSeconds = 86400;
};

struct ParsedArguments
{
    AppMode appMode = AppMode::None;
    std::wstring strServiceName = L"ServiceWrapper";
    std::wstring strDisplayName;
    std::wstring strTargetBinaryPath;
    std::wstring strTargetArguments;
    std::wstring strAccount = L"LocalSystem";
    std::wstring strPassword;
    StartupType startupType = StartupType::Manual;
    RestartOptions restartOptions;
};

// -----------------------------------------------------------------------------

ParsedArguments ParseArguments(int nArgc, wchar_t* ppszArgv[]);
std::wstring BuildUsageText();
