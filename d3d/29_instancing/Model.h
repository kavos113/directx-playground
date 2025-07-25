#ifndef MODEL_H
#define MODEL_H

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include <vector>
#include <array>
#include <string>


class Model
{
public:
    Model(
        Microsoft::WRL::ComPtr<ID3D12Device> device,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap,
        RECT rc
    );
    void cleanup();
    void executeBarrier(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) const;

    void render(const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> &commandList);

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
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
                    .SemanticName = "NORMAL",
                    .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                    .InputSlot = 0,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
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
                },

                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "WORLD",
                    .SemanticIndex = 0,
                    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                    .InputSlot = 1,
                    .AlignedByteOffset = 0,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                    .InstanceDataStepRate = 1
                },
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "WORLD",
                    .SemanticIndex = 1,
                    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                    .InputSlot = 1,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                    .InstanceDataStepRate = 1
                },
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "WORLD",
                    .SemanticIndex = 2,
                    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                    .InputSlot = 1,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                    .InstanceDataStepRate = 1
                },
                D3D12_INPUT_ELEMENT_DESC{
                    .SemanticName = "WORLD",
                    .SemanticIndex = 3,
                    .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                    .InputSlot = 1,
                    .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
                    .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA,
                    .InstanceDataStepRate = 1
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

private:

    struct MatrixBuffer
    {
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX projection;
    };

    struct LightBuffer
    {
        DirectX::XMFLOAT3 direction;
        float padding;
        DirectX::XMFLOAT3 ambient;
    };

    struct InstanceData
    {
        DirectX::XMFLOAT4X4 world;
    };

    void createCopyCommands();

    void loadModel(const std::string &path);
    void createVertexBuffer();
    void createIndexBuffer();
    void createMatrixBuffer(RECT rc);
    void createLightBuffer();

    void createInstanceBuffer();

    void loadTexture(const std::wstring& path);
    void createBuffer(
        UINT64 size,
        ID3D12Resource **buffer,
        D3D12_HEAP_TYPE heapType,
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
    );

    static constexpr int NUM_INSTANCES = 100;

    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descHeap;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_copyCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_copyCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_copyCommandList;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_copyFence;
    UINT64 m_copyFenceValue = 0;
    HANDLE m_copyFenceEvent = nullptr;

    std::vector<Vertex> m_vertices;
    std::vector<unsigned short> m_indices;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_matrixBuffer;
    MatrixBuffer *m_matrixBufferData = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_lightBuffer;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_waitForCopyResources;
    std::vector<D3D12_RESOURCE_BARRIER> m_barriers;

    std::array<InstanceData, NUM_INSTANCES> m_instanceData{};
    Microsoft::WRL::ComPtr<ID3D12Resource> m_instanceBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView = {};

    const std::string MODEL_PATH = "security_camera_01_2k.obj";
    const std::wstring TEXTURE_PATH = L"textures/security_camera_01_diff_2k.jpg";
};



#endif //MODEL_H
