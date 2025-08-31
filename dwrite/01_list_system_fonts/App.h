#ifndef DWRITE_01_LIST_SYSTEM_FONTS_APP_H
#define DWRITE_01_LIST_SYSTEM_FONTS_APP_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <wrl/client.h>

class IDWriteFactory;

class App
{
public:
    App();
    ~App();

    void run();
private:

    HMODULE m_dwriteModule;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_factory;
};


#endif //DWRITE_01_LIST_SYSTEM_FONTS_APP_H