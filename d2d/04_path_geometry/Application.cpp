#include "Application.h"

#include <array>

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

    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    std::array featureLevels = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    Microsoft::WRL::ComPtr<ID3D11Device> d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3dContext;
    hr = D3D11CreateDevice(
        nullptr, // Use default adapter
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr, // No software device
        creationFlags,
        featureLevels.data(), static_cast<UINT>(featureLevels.size()),
        D3D11_SDK_VERSION,
        &d3dDevice,
        nullptr, // Feature level not needed
        &d3dContext
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D3D11 device", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    hr = d3dDevice.As(&dxgiDevice);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to get IDXGIDevice", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D device", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    hr = m_d2dDevice->CreateDeviceContext(
        D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
        &m_d2dContext
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D device context", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
        .Width = size.width,
        .Height = size.height,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .Stereo = false,
        .SampleDesc = { 1, 0 }, 
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2, // Double buffering
        .Scaling = DXGI_SCALING_NONE,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        .Flags = 0
    };

    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to get DXGI adapter", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to get DXGI factory", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    hr = dxgiFactory->CreateSwapChainForHwnd(
        d3dDevice.Get(),
        m_hwnd,
        &swapChainDesc,
        nullptr, // No additional swap chain options
        nullptr, // No restrict to output
        &m_swapChain
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create swap chain", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    createSurfaceBitmap();

    createResources();
}

void Application::createSurfaceBitmap()
{
    Microsoft::WRL::ComPtr<IDXGISurface> dxgiSurface;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiSurface));
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to get DXGI surface", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    hr = m_d2dContext->CreateBitmapFromDxgiSurface(
        dxgiSurface.Get(),
        D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            96.0f, // Default DPI
            96.0f  // Default DPI
        ),
        &m_bitmap
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D bitmap from DXGI surface", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    m_d2dContext->SetTarget(m_bitmap.Get());
}

void Application::createResources()
{
    HRESULT hr = m_d2dContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Red),
        &m_brush
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D solid color brush", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    hr = m_d2dContext->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::Green),
        &m_greenBrush
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D green brush", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties = {
        .startCap = D2D1_CAP_STYLE_ROUND,
        .endCap = D2D1_CAP_STYLE_SQUARE,
        .dashCap = D2D1_CAP_STYLE_TRIANGLE,
        .lineJoin = D2D1_LINE_JOIN_MITER,
        .miterLimit = 10.0f,
        .dashStyle = D2D1_DASH_STYLE_CUSTOM,
        .dashOffset = 0.0f,
    };
    std::array dashes = { 5.0f, 2.0f };
    hr = m_d2dFactory->CreateStrokeStyle(
        &strokeStyleProperties,
        dashes.data(),
        dashes.size(),
        &m_strokeStyle
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D stroke style", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    hr = m_d2dFactory->CreatePathGeometry(&m_pathGeometry);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to create D2D path geometry", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
    hr = m_pathGeometry->Open(&sink);
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to open D2D geometry sink", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    sink->SetFillMode(D2D1_FILL_MODE_WINDING);
    sink->BeginFigure(
        D2D1::Point2F(100.0f, 100.0f),
        D2D1_FIGURE_BEGIN_FILLED
    );
    sink->AddLine(D2D1::Point2F(200.0f, 100.0f));
    sink->AddArc(D2D1::ArcSegment(
        D2D1::Point2F(200.0f, 200.0f),
        D2D1::SizeF(50.0f, 50.0f),
        0.0f, // Rotation angle
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL
    ));
    sink->AddLine(D2D1::Point2F(100.0f, 200.0f));
    sink->AddArc(D2D1::ArcSegment(
        D2D1::Point2F(100.0f, 100.0f),
        D2D1::SizeF(50.0f, 50.0f),
        0.0f, // Rotation angle
        D2D1_SWEEP_DIRECTION_CLOCKWISE,
        D2D1_ARC_SIZE_SMALL
    ));
    sink->EndFigure(D2D1_FIGURE_END_CLOSED);
    hr = sink->Close();
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to close D2D geometry sink", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
}

void Application::onPaint()
{
    PAINTSTRUCT ps;
    BeginPaint(m_hwnd, &ps);

    m_d2dContext->BeginDraw();
    m_d2dContext->Clear(D2D1::ColorF(D2D1::ColorF::SkyBlue));

    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
    m_d2dContext->DrawGeometry(
        m_pathGeometry.Get(),
        m_brush.Get(),
        2.0f, // Stroke width
        m_strokeStyle.Get()
    );
    m_d2dContext->FillGeometry(
        m_pathGeometry.Get(),
        m_greenBrush.Get()
    );

    if (FAILED(m_d2dContext->EndDraw()))
    {
        m_d2dContext.Reset();
        m_brush.Reset();
    }

    m_swapChain->Present(1, 0);

    EndPaint(m_hwnd, &ps);
}

void Application::onResize()
{
    if (!m_d2dContext)
    {
        return;
    }

    RECT rc;
    GetClientRect(m_hwnd, &rc);

    D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

    m_d2dContext->SetTarget(nullptr);
    m_bitmap.Reset();

    HRESULT hr = m_swapChain->ResizeBuffers(
        2, // Double buffering
        size.width, size.height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        0 // No additional flags
    );
    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Failed to resize swap chain buffers", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    createSurfaceBitmap();

    InvalidateRect(m_hwnd, nullptr, FALSE);
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