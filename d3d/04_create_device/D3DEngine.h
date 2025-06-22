#ifndef D3D_01_HELLO_WORLD_D3DENGINE_H
#define D3D_01_HELLO_WORLD_D3DENGINE_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

class D3DEngine
{
public:
    explicit D3DEngine(HWND hwnd);
    ~D3DEngine();

    void cleanup();

    void render();

private:
    static void enableDebugLayer();

    void createDXGIFactory();
    void getAdapter();

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
