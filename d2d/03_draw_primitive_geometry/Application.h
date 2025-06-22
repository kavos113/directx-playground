#ifndef WIN32_02_SPLIT_CLASS_APPLICATION_H
#define WIN32_02_SPLIT_CLASS_APPLICATION_H

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi1_6.h>
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

    void createSurfaceBitmap();
    void createResources();

    HWND m_hwnd;

    const wchar_t* className = L"ApplicationWindowClass";

    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_d2dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_bitmap;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_brush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_greenBrush;
    Microsoft::WRL::ComPtr<ID2D1StrokeStyle> m_strokeStyle;
};



#endif //WIN32_02_SPLIT_CLASS_APPLICATION_H
