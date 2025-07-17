#ifndef D3D_01_HELLO_WORLD_D3DENGINE_H
#define D3D_01_HELLO_WORLD_D3DENGINE_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <array>
#include <memory>
#include <vector>

#include "Debug.h"

class D3DEngine
{
public:
    explicit D3DEngine(HWND hwnd);
    ~D3DEngine();

    void cleanup();

    void render();

private:
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
    };

    void createDXGIFactory();
    void getAdapter(IDXGIAdapter1 **adapter);
    void createDevice();
    void createCommandResources();
    void createSwapChain(HWND hwnd);
    void createSwapChainResources();
    void createFence();

    void createBuffer(
        ID3D12Resource **buffer,
        size_t size,
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ
    );

    void createVertexBuffer();

    void createAS();

    void beginFrame(UINT frameIndex);
    void recordCommands(UINT frameIndex);
    void endFrame(UINT frameIndex);

    void waitForFence(UINT frameIndex);

    std::unique_ptr<Debug> m_debug;

    static constexpr UINT FRAME_COUNT = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device5> m_device;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::array<float, 4> m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    std::array<Microsoft::WRL::ComPtr<ID3D12Fence>, FRAME_COUNT> m_fence;
    std::array<UINT64, FRAME_COUNT> m_fenceValues = {};
    std::array<HANDLE, FRAME_COUNT> m_fenceEvents = {};

    const std::vector<Vertex> m_vertices = {
        {{0.0f, 0.5f, 0.0f},},
        {{-0.5f, -0.5f, 0.0f},},
        {{0.5f, -0.5f, 0.0f},}
    };

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blas;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlas;
    uint32_t m_tlasSize = 0;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
