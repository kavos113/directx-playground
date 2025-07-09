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
        DirectX::XMFLOAT2 uv;

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
                    .SemanticName = "TEXCOORD",
                    .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32_FLOAT,
                    .InputSlot = 0,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                    .InstanceDataStepRate = 0
                }
            };
        }

        bool operator< (const Vertex &other) const
        {
            return position.x < other.position.x ||
                   (position.x == other.position.x && position.y < other.position.y) ||
                   (position.x == other.position.x && position.y == other.position.y && position.z < other.position.z) ||
                   (position.x == other.position.x && position.y == other.position.y && position.z == other.position.z && uv.x < other.uv.x) ||
                   (position.x == other.position.x && position.y == other.position.y && position.z == other.position.z && uv.x == other.uv.x && uv.y < other.uv.y);
        }
    };

    struct MatrixBuffer
    {
        DirectX::XMMATRIX world;
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;

        MatrixBuffer()
            : world(DirectX::XMMatrixIdentity()),
              view(DirectX::XMMatrixIdentity()),
              projection(DirectX::XMMatrixIdentity())
        {}
    };

    void createDXGIFactory();
    void getAdapter(IDXGIAdapter1 **adapter) const;
    void createDevice();
    void createCommandResources();
    void createSwapChain(HWND hwnd);
    void createSwapChainResources();
    void createDepthResources(UINT width, UINT height);
    void createFence();

    void loadModel(const std::string &path);
    void createVertexBuffer();
    void createIndexBuffer();
    void createMatrixBuffer(RECT rc);
    void createDescriptorHeap();

    static Microsoft::WRL::ComPtr<ID3D10Blob> compileShader(
        const wchar_t *fileName,
        const char *entryPoint,
        const char *target,
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG
    );

    void createPipelineState();
    void createViewport(HWND hwnd);

    void loadTexture(const std::wstring& path);
    void createBuffer(
        UINT64 size,
        ID3D12Resource **buffer,
        D3D12_HEAP_TYPE heapType,
        D3D12_TEXTURE_LAYOUT layout,
        D3D12_RESOURCE_STATES initialState
    );
    void copyTexture(
        const Microsoft::WRL::ComPtr<ID3D12Resource> &srcBuffer,
        const Microsoft::WRL::ComPtr<ID3D12Resource> &dstBuffer
    ) const;
    void copyBuffer(
        const Microsoft::WRL::ComPtr<ID3D12Resource> &srcBuffer,
        const Microsoft::WRL::ComPtr<ID3D12Resource> &dstBuffer
    ) const;
    void executeCopy();

    void barrier(
        const Microsoft::WRL::ComPtr<ID3D12Resource> &resource,
        D3D12_RESOURCE_STATES beforeState,
        D3D12_RESOURCE_STATES afterState
    ) const;

    void beginFrame(UINT frameIndex);
    void recordCommands(UINT frameIndex) const;
    void endFrame(UINT frameIndex);

    void waitForFence(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& queue, UINT frameIndex);

    std::unique_ptr<Debug> m_debug;

    static constexpr UINT FRAME_COUNT = 2;

    const std::string MODEL_PATH = "security_camera_01_2k.obj";
    const std::wstring TEXTURE_PATH = L"textures/security_camera_01_diff_2k.jpg";

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, FRAME_COUNT> m_commandAllocators;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_copyCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_copyCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_copyCommandList;

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_backBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::array<float, 4> m_clearColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FRAME_COUNT> m_depthBuffers;

    std::array<Microsoft::WRL::ComPtr<ID3D12Fence>, FRAME_COUNT> m_fence;
    std::array<UINT64, FRAME_COUNT> m_fenceValues = {};
    std::array<HANDLE, FRAME_COUNT> m_fenceEvents = {};

    std::vector<Vertex> m_vertices;
    std::vector<unsigned short> m_indices;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    D3D12_VIEWPORT m_viewport = {};
    D3D12_RECT m_scissorRect = {};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;

    float m_angle = 0.0f;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_matrixBuffer;
    MatrixBuffer *m_matrixBufferData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_waitForCopyResources;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
