#ifndef UNICODE
#define UNICODE
#endif

#include <iostream>
#include <map>
#include <set>
#include <windows.h>

#include <string>

struct SystemFontInfo
{
    std::wstring family;
};

const WCHAR* FONT_REG_PATH = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

static int CALLBACK EnumFontFamExProc(
    const LOGFONT* lpelfe,
    const TEXTMETRIC* lpntme,
    DWORD FontType,
    LPARAM lParam
)
{
    auto* fonts = reinterpret_cast<std::set<std::wstring>*>(lParam);
    if (fonts == nullptr)
    {
        return 0;
    }

    fonts->insert(lpelfe->lfFaceName);

    return 1;
}

int main()
{
    std::set<std::wstring> fonts;
    HDC hdc = GetDC(nullptr);

    LOGFONTW logfont = {};
    logfont.lfCharSet = DEFAULT_CHARSET;

    EnumFontFamiliesExW(hdc, &logfont, EnumFontFamExProc, reinterpret_cast<LPARAM>(&fonts), 0);

    ReleaseDC(nullptr, hdc);

    std::map<std::wstring, std::wstring> fontRegistryMap;

    HKEY hKey;
    if (RegOpenKeyW(HKEY_LOCAL_MACHINE, FONT_REG_PATH, &hKey) != ERROR_SUCCESS)
    {
        std::cerr << "Failed to open registry key." << std::endl;
        return 1;
    }

    DWORD valueCount = 0;
    RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &valueCount, nullptr, nullptr, nullptr, nullptr);

    for (DWORD i = 0; i < valueCount; ++i)
    {
        WCHAR valueName[256];
        DWORD valueNameSize = 256;
        BYTE data[MAX_PATH];
        DWORD dataSize = sizeof(data);
        DWORD type;

        if (RegEnumValueW(hKey, i, valueName, &valueNameSize, nullptr, &type, data, &dataSize) != ERROR_SUCCESS)
        {
            continue;
        }

        if (type != REG_SZ)
        {
            continue;
        }

        std::wstring familyName(valueName, valueNameSize);
        size_t pos = familyName.find(L" (");
        if (pos != std::wstring::npos)
        {
            familyName = familyName.substr(0, pos);
        }

        fontRegistryMap[familyName] = reinterpret_cast<WCHAR*>(data);
    }
    RegCloseKey(hKey);

    WCHAR systemDir[MAX_PATH];
    GetWindowsDirectoryW(systemDir, MAX_PATH);
    std::wstring fontDir = std::wstring(systemDir) + L"\\Fonts\\";

    std::cout << "Font count: " << fonts.size() << "\n"
                << "Registry font count: " << fontRegistryMap.size() << std::endl;
    for (const auto& font : fonts)
    {
        OutputDebugStringW((L"Font: " + font + L"\n").c_str());
    }

    for (const auto& [family, path] : fontRegistryMap)
    {
        OutputDebugStringW((L"Registry Font: " + family + L" -> " + path + L"\n").c_str());
    }

    return 0;
}