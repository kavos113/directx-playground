#ifndef WIN32_02_SPLIT_CLASS_APPLICATION_H
#define WIN32_02_SPLIT_CLASS_APPLICATION_H

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1.h>
#include <wrl/client.h>

class Application
{
public:
    Application();
    ~Application();

    int createWindow(int x = CW_USEDEFAULT, int y = CW_USEDEFAULT, int width = 800, int height = 600);

    void initD2D();

    void run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void onPaint();
    void onResize();

    HWND m_hwnd;

    const wchar_t* className = L"ApplicationWindowClass";

    Microsoft::WRL::ComPtr<ID2D1Factory> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> m_renderTarget;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;
};



#endif //WIN32_02_SPLIT_CLASS_APPLICATION_H
