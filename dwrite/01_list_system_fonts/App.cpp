#include "App.h"

#include <dwrite.h>
#include <iostream>
#include <string>
#include <vector>

using PFN_DwriteCreateFactory = HRESULT(WINAPI*)(
    DWRITE_FACTORY_TYPE factoryType,
    REFIID iid,
    IUnknown** factory
);

struct FontInfo
{
    std::wstring family;
    std::wstring styleName;
    DWRITE_FONT_WEIGHT weight;
    DWRITE_FONT_STYLE style;
    DWRITE_FONT_STRETCH stretch;
};

App::App()
{
    m_dwriteModule = LoadLibraryW(L"dwrite.dll");
    if (!m_dwriteModule)
    {
        std::cerr << "Failed to load dwrite.dll" << std::endl;
        return;
    }

    PFN_DwriteCreateFactory pDwriteCreateFactory =
        reinterpret_cast<PFN_DwriteCreateFactory>(
            GetProcAddress(m_dwriteModule, "DWriteCreateFactory")
        );
    if (!pDwriteCreateFactory)
    {
        std::cerr << "Failed to get DWriteCreateFactory address." << std::endl;
        FreeLibrary(m_dwriteModule);
        m_dwriteModule = nullptr;
        return;
    }

    HRESULT hr = pDwriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_factory.GetAddressOf())
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create DirectWrite factory." << std::endl;
        FreeLibrary(m_dwriteModule);
        m_dwriteModule = nullptr;
        return;
    }

    std::cout << "DirectWrite factory created successfully." << std::endl;
}

App::~App()
{
    m_factory.Reset();
    if (m_dwriteModule)
    {
        FreeLibrary(m_dwriteModule);
        m_dwriteModule = nullptr;
    }
}

void App::run()
{
    Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection;
    HRESULT hr = m_factory->GetSystemFontCollection(&fontCollection);
    if (FAILED(hr))
    {
        std::cerr << "Failed to get system font collection." << std::endl;
        return;
    }

    UINT32 fontCount = fontCollection->GetFontFamilyCount();
    std::wcout << L"Number of font families: " << fontCount << std::endl;

    WCHAR localeName[LOCALE_NAME_MAX_LENGTH];
    int localeLength = GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH);
    if (localeLength == 0)
    {
        std::wcerr << L"Failed to get user default locale name." << std::endl;
        wcscpy_s(localeName, L"en-US");
    }

    std::vector<FontInfo> fonts;

    for (UINT32 i = 0; i < fontCount; ++i)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        hr = fontCollection->GetFontFamily(i, &fontFamily);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get font family." << std::endl;
            continue;
        }

        Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> familyNames;
        hr = fontFamily->GetFamilyNames(&familyNames);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get family names." << std::endl;
            continue;
        }
        UINT32 index = 0;
        BOOL exists = FALSE;
        hr = familyNames->FindLocaleName(localeName, &index, &exists);
        if (FAILED(hr))
        {
            std::cerr << "Failed to find locale name." << std::endl;
            continue;
        }
        if (!exists)
        {
            hr = familyNames->FindLocaleName(L"en-US", &index, &exists);
            if (FAILED(hr) || !exists)
            {
                index = 0; // Fallback to the first entry
            }
        }
        UINT32 length = 0;
        hr = familyNames->GetStringLength(index, &length);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get string length." << std::endl;
            continue;
        }
        std::vector<WCHAR> nameBuffer(length + 1);
        hr = familyNames->GetString(index, nameBuffer.data(), length + 1);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get family name string." << std::endl;
            continue;
        }
        std::wstring familyName(nameBuffer.data());

        UINT32 numFonts = fontFamily->GetFontCount();
        for (UINT32 j = 0; j < numFonts; ++j)
        {
            Microsoft::WRL::ComPtr<IDWriteFont> font;
            hr = fontFamily->GetFont(j, &font);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get font." << std::endl;
                continue;
            }

            DWRITE_FONT_WEIGHT weight = font->GetWeight();
            DWRITE_FONT_STYLE style = font->GetStyle();
            DWRITE_FONT_STRETCH stretch = font->GetStretch();

            Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> names;
            hr = font->GetFaceNames(&names);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get localized names." << std::endl;
                continue;
            }

            index = 0;
            exists = FALSE;
            hr = names->FindLocaleName(localeName, &index, &exists);
            if (FAILED(hr))
            {
                std::cerr << "Failed to find locale name in face names." << std::endl;
                continue;
            }
            if (!exists)
            {
                hr = names->FindLocaleName(L"en-US", &index, &exists);
                if (FAILED(hr) || !exists)
                {
                    index = 0; // Fallback to the first entry
                }
            }
            length = 0;
            hr = names->GetStringLength(index, &length);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get face name string length." << std::endl;
                continue;
            }

            std::vector<WCHAR> styleBuffer(length + 1);
            hr = names->GetString(index, styleBuffer.data(), length + 1);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get face name string." << std::endl;
                continue;
            }

            fonts.push_back({ familyName, std::wstring(styleBuffer.data()), weight, style, stretch });
        }
    }

    std::wcout << L"Total fonts found: " << fonts.size() << std::endl;
    for (const auto& font : fonts)
    {
        std::wcout << L"Family: " << font.family
                   << L", Style: " << font.styleName
                   << L", Weight: " << font.weight
                   << L", StyleType: " << font.style
                   << L", Stretch: " << font.stretch
                   << std::endl;
    }
}
