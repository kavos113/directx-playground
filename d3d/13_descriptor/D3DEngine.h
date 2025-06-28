#ifndef D3D_01_HELLO_WORLD_D3DENGINE_H
#define D3D_01_HELLO_WORLD_D3DENGINE_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>
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
        DirectX::XMFLOAT4 color;

        static auto inputLayout()
        {
            return std::array{
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "POSITION",
                    .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                    .InputSlot = 0,
                    .AlignedByteOffset = 0,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    .InstanceDataStepRate = 0
                },
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "COLOR",
                    .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                    .InputSlot = 0,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    .InstanceDataStepRate = 0
                }
            };
        }
    };

    void createDXGIFactory();
    void getAdapter(IDXGIAdapter1 **adapter);
    void createDevice();
    void createCommandResources();
    void createSwapChain(HWND hwnd);
    void createSwapChainResources();
    void createFence();

    void createVertexBuffer();
    void createIndexBuffer();
    void createColorBuffer();
    void createDescriptorHeap();

    static Microsoft::WRL::ComPtr<ID3D10Blob> compileShader(
        const wchar_t *fileName,
        const char *entryPoint,
        const char *target,
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG
    );

    void createPipelineState();
    void createViewport(HWND hwnd);

    void beginFrame(UINT frameIndex);
    void recordCommands(UINT frameIndex) const;
    void endFrame(UINT frameIndex);

    void waitForFence();

    std::unique_ptr<Debug> m_debug;

    static constexpr UINT FRAME_COUNT = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::array<float, 4> m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    const std::vector<Vertex> m_vertices = {
        {
            {-0.5, -0.5f, 0.0f},
            {1.0f, 0.0f, 0.0f, 1.0f}
        },
        {
            {-0.5f, 0.5f, 0.0f},
            {0.0f, 1.0f, 0.0f, 1.0f}
        },
        {
            {0.5f, -0.5f, 0.0f},
            {0.0f, 0.0f, 1.0f, 1.0f}
        },
        {
            {0.5f, 0.5f, 0.0f},
            {1.0f, 1.0f, 0.0f, 1.0f}
        }
    };
    const std::vector<unsigned short> m_indices = {
        0, 1, 2,
        2, 1, 3
    };

    const std::vector<float> m_color = {
        1.0f, 1.0f, 0.0f, 1.0f
    };

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT m_scissorRect = {};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_colorBuffer;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
