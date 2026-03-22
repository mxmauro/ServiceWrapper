#pragma once

#include <string>
#include <vector>
// -----------------------------------------------------------------------------

bool StrNoCaseEqual(const std::wstring& strLeft, const std::wstring& strRight);
std::wstring QuoteForCommandLine(const std::wstring& strValue);
std::wstring JoinCommandLine(const std::vector<std::wstring>& vecParts);
std::wstring GetCurrentModulePath();
std::wstring GetDirectoryFromPath(const std::wstring& strFilePath);
std::wstring GetLastErrorMessage(DWORD dwOsError);
