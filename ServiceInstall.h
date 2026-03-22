#pragma once

#include <Windows.h>
#include "CommandLine.h"

// -----------------------------------------------------------------------------

DWORD InstallService(const ParsedArguments& argsParsed);
DWORD UninstallService(const ParsedArguments &argsParsed);
