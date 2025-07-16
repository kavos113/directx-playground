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
#include <D3D12MemAlloc.h>

#include <array>
#include <memory>

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
    void getAdapter(IDXGIAdapter1 **adapter) const;
    void createDevice();
    void createCommandResources();
    void createSwapChain(HWND hwnd);
    void createSwapChainResources();
    void createDepthResources(UINT width, UINT height);
    void createFence();

    void createDescriptorHeap();

    static Microsoft::WRL::ComPtr<ID3D10Blob> compileShader(
        const wchar_t *fileName,
        const char *entryPoint,
        const char *target,
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG
    );

    void createPipelineState();
    void createViewport(HWND hwnd);

    void barrier(
        const Microsoft::WRL::ComPtr<ID3D12Resource> &resource,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState
    ) const;

    void beginFrame(UINT frameIndex);
    void recordCommands(UINT frameIndex) const;
    void endFrame(UINT frameIndex);

    void waitForFence(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& queue, UINT frameIndex);
    void executeCommand(UINT frameIndex);

    std::unique_ptr<Debug> m_debug;
    std::unique_ptr<Model> m_model;

    static constexpr UINT FRAME_COUNT = 2;

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::array<float, 4> m_clearColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    std::array<Microsoft::WRL::ComPtr<D3D12MA::Allocation>, FRAME_COUNT> m_depthBuffers;

    std::array<Microsoft::WRL::ComPtr<ID3D12Fence>, FRAME_COUNT> m_fence;
    std::array<UINT64, FRAME_COUNT> m_fenceValues = {};
    std::array<HANDLE, FRAME_COUNT> m_fenceEvents = {};

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT m_scissorRect = {};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;

    Microsoft::WRL::ComPtr<D3D12MA::Allocator> m_allocator;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
