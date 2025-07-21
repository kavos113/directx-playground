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
#include "Model.h"

class D3DEngine
{
public:
    explicit D3DEngine(HWND hwnd);
    ~D3DEngine();

    void cleanup();

    void render();

private:
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

    void createVertexBuffers();

    void createAS();
    void createRaytracingPipelineState();
    void createShaderTable();
    void createRaytracingResources();

    void updateTLAS();

    void beginFrame(UINT frameIndex);
    void recordCommands(UINT frameIndex);
    void endFrame(UINT frameIndex);

    void executeCommand(UINT frameIndex);

    void waitForFence(UINT frameIndex);

    std::unique_ptr<Debug> m_debug;
    std::unique_ptr<Model> m_model;

    static constexpr UINT FRAME_COUNT = 2;

    RECT m_windowRect = {};

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device5> m_device;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> m_commandList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
    std::array<float, 4> m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

    std::array<Microsoft::WRL::ComPtr<ID3D12Fence>, FRAME_COUNT> m_fence;
    std::array<UINT64, FRAME_COUNT> m_fenceValues = {};
    std::array<HANDLE, FRAME_COUNT> m_fenceEvents = {};

    const std::vector<DirectX::XMFLOAT3> m_planeVertices = {
        {-10.0f, -1.0f, -10.0f},
        {10.0f, -1.0f, -10.0f},
        {10.0f, -1.0f, 10.0f},
        {-10.0f, -1.0f, 10.0f},
    };

    const std::vector<uint32_t> m_planeIndices = {
        0, 1, 2,
        0, 2, 3
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_blas;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_planeVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_planeIndexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_planeBlas;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlas;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_instanceDescBuffer;
    uint32_t m_tlasSize = 0;

    Microsoft::WRL::ComPtr<ID3D12StateObject> m_raytracingPipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shaderTable;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_raytracingOutput;
    UINT m_shaderRecordSize = 0;

    float m_angle = 0.0f;

    const wchar_t *SHADER_FILE = L"shader.hlsl";
    const wchar_t *RAYGEN_SHADER = L"RayGen";
    const wchar_t *MISS_SHADER = L"MissShader";
    const wchar_t *CLOSE_SHADER = L"ClosestHitShader";
    const wchar_t *HIT_GROUP = L"HitGroup";
    const wchar_t *PLANE_CLOSE_SHADER = L"PlaneClosestHitShader";
    const wchar_t *PLANE_HIT_GROUP = L"PlaneHitGroup";
    const wchar_t *SHADOW_MISS_SHADER = L"ShadowMissShader";
    const wchar_t *SHADOW_CLOSE_SHADER = L"ShadowClosestHitShader";
    const wchar_t *SHADOW_HIT_GROUP = L"ShadowHitGroup";

    struct BuiltInTriangleIntersectionAttributes
    {
        DirectX::XMFLOAT2 barycentrics;
    };

    struct RayTracingPayload
    {
        DirectX::XMFLOAT4 color;
    };
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
