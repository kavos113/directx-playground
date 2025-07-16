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
    D3DEngine();
    ~D3DEngine();

    void run();

private:
    static void enableDebugLayer();

    void createDXGIFactory();
    void getAdapter(IDXGIAdapter1 **adapter);
    void createDevice();

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
