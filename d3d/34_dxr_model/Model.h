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
    void createView();

    void cleanup();
    void executeBarrier(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) const;

    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc() const
    {
        return m_geometryDesc;
    }

    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT2 uv;

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
    void createCopyCommands();

    void loadModel(const std::string &path);
    void createVertexBuffer();
    void createIndexBuffer();
    void createGeometryDesc();

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

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_RAYTRACING_GEOMETRY_DESC m_geometryDesc = {};

    Microsoft::WRL::ComPtr<ID3D12Resource> m_texture;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_waitForCopyResources;
    std::vector<D3D12_RESOURCE_BARRIER> m_barriers;

    const std::string MODEL_PATH = "security_camera_01_2k.obj";
    const std::wstring TEXTURE_PATH = L"textures/security_camera_01_diff_2k.jpg";
};



#endif //MODEL_H
