#include "Application.h"

void Application::initD2D()
{
    HRESULT hr = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        m_d2dFactory.GetAddressOf()
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D factory", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    hr = m_d2dFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(m_hwnd, size),
        m_renderTarget.GetAddressOf()
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create render target", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Green);
    hr = m_renderTarget->CreateSolidColorBrush(color, m_brush.GetAddressOf());
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create solid color brush", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
}

void Application::onPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(m_hwnd, &ps);

    m_renderTarget->BeginDraw();
    m_renderTarget->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));
    m_renderTarget->FillRectangle(D2D1::RectF(100, 100, 300, 300), m_brush.Get());

    HRESULT hr = m_renderTarget->EndDraw();
    if (FAILED(hr))
    {
        m_renderTarget.Reset();
        m_brush.Reset();
    }

    EndPaint(m_hwnd, &ps);
}

void Application::onResize()
{
    if (m_renderTarget)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        if (FAILED(m_renderTarget->Resize(size)))
        {
            m_renderTarget.Reset();
            m_brush.Reset();
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

Application::Application()
    : m_hwnd(nullptr)
{
    WNDCLASSEX wc = {
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = WindowProc,
        .hInstance = GetModuleHandle(nullptr),
        .hIcon = LoadIcon(nullptr, IDI_APPLICATION),
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = reinterpret_cast<HBRUSH>((COLOR_WINDOW + 1)),
        .lpszClassName = className,
    };

    RegisterClassEx(&wc);
}

Application::~Application()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    UnregisterClass(className, GetModuleHandle(nullptr));
}

int Application::createWindow(int x, int y, int width, int height)
{
    HWND m_hwnd = CreateWindowEx(
        0,
        className,
        L"Hello, World!",
        WS_OVERLAPPEDWINDOW,
        x, y,
        width, height,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );
    if (m_hwnd == nullptr)
    {
        return -1;
    }

    return 0;
}

void Application::run()
{
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT Application::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Application *app = nullptr;

    if (uMsg == WM_NCCREATE)
    {
        CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
        app = reinterpret_cast<Application *>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

        app->m_hwnd = hwnd;
    }
    else
    {
        app = reinterpret_cast<Application *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (app)
    {
        return app->handleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Application::handleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
        onPaint();
        return 0;

    case WM_SIZE:
        onResize();
        return 0;

    default:
        return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
    }
}