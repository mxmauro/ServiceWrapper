#include <Windows.h>
#include <cwchar>
#include <memory>
#include "ScopedResource.h"
#include "StringUtils.h"

// -----------------------------------------------------------------------------

inline void LocalFreeDeleter(wchar_t *p) noexcept
{
    if (p)
    {
        ::LocalFree(static_cast<HLOCAL>(p));
    }
}

using ScopedResourceLocalFree = ScopedResource<wchar_t *, LocalFreeDeleter>;

// -----------------------------------------------------------------------------

static std::wstring TrimTrailingLineBreaks(const std::wstring& strValue);

// -----------------------------------------------------------------------------

bool StrNoCaseEqual(const std::wstring& strLeft, const std::wstring& strRight)
{
    return _wcsicmp(strLeft.c_str(), strRight.c_str()) == 0;
}

std::wstring QuoteForCommandLine(const std::wstring& strValue)
{
    bool bNeedsQuotes = false;
    std::wstring strResult;
    size_t nSlashCount = 0;

    if (strValue.empty())
    {
        return L"\"\"";
    }

    bNeedsQuotes = strValue.find_first_of(L" \t\"") != std::wstring::npos;
    if (!bNeedsQuotes)
    {
        return strValue;
    }

    strResult.push_back(L'"');

    for (const wchar_t chValue : strValue)
    {
        if (chValue == L'\\')
        {
            ++nSlashCount;
            continue;
        }

        if (chValue == L'"')
        {
            strResult.append(nSlashCount * 2 + 1, L'\\');
            strResult.push_back(chValue);
            nSlashCount = 0;
            continue;
        }

        if (nSlashCount != 0)
        {
            strResult.append(nSlashCount, L'\\');
            nSlashCount = 0;
        }

        strResult.push_back(chValue);
    }

    if (nSlashCount != 0)
    {
        strResult.append(nSlashCount * 2, L'\\');
    }

    strResult.push_back(L'"');
    return strResult;
}

std::wstring JoinCommandLine(const std::vector<std::wstring>& vecParts)
{
    std::wstring strResult;

    for (const auto& strPart : vecParts)
    {
        if (!strResult.empty())
        {
            strResult.push_back(L' ');
        }

        strResult.append(QuoteForCommandLine(strPart));
    }

    return strResult;
}

std::wstring GetCurrentModulePath()
{
    std::wstring strBuffer(MAX_PATH, L'\0');
    DWORD dwLength = 0;

    for (;;)
    {
        dwLength = GetModuleFileNameW(nullptr, strBuffer.data(), static_cast<DWORD>(strBuffer.size()));
        if (dwLength == 0)
        {
            return {};
        }

        if (dwLength < strBuffer.size() - 1)
        {
            strBuffer.resize(dwLength);
            return strBuffer;
        }

        strBuffer.resize(strBuffer.size() * 2);
    }
}

std::wstring GetDirectoryFromPath(const std::wstring& strFilePath)
{
    size_t nSeparator = 0;

    nSeparator = strFilePath.find_last_of(L"\\/");
    if (nSeparator == std::wstring::npos)
    {
        return {};
    }

    return strFilePath.substr(0, nSeparator);
}

std::wstring GetLastErrorMessage(DWORD dwOsError)
{
    LPWSTR pszRawMessage = nullptr;
    DWORD dwLength = 0;
    DWORD dwFlags;
    ScopedResourceLocalFree cRawMessageHolder;

    dwFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    dwLength = FormatMessageW(dwFlags, nullptr, dwOsError, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                              reinterpret_cast<LPWSTR>(&pszRawMessage), 0, nullptr);
    cRawMessageHolder.reset(pszRawMessage);
    if (dwLength == 0 || (!pszRawMessage))
    {
        return L"Windows error " + std::to_wstring(dwOsError);
    }

    return TrimTrailingLineBreaks(std::wstring(pszRawMessage, dwLength));
}

// -----------------------------------------------------------------------------

static std::wstring TrimTrailingLineBreaks(const std::wstring& strValue)
{
    std::wstring strResult = strValue;

    while (!strResult.empty() && (strResult.back() == L'\r' || strResult.back() == L'\n'))
    {
        strResult.pop_back();
    }

    return strResult;
}
