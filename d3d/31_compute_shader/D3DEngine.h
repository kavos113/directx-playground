#ifndef D3D_01_HELLO_WORLD_D3DENGINE_H
#define D3D_01_HELLO_WORLD_D3DENGINE_H

#ifndef UNICODE
#define UNICODE
#include <array>
#endif
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
    struct Data
    {
        float value;
        float id;
    };

    static void enableDebugLayer();

    void createDXGIFactory();
    void getAdapter(IDXGIAdapter1 **adapter);
    void createDevice();
    void createCommandResources();
    void createDescriptorHeap();
    void createResources();
    void createPipeline();

    Microsoft::WRL::ComPtr<IDXGIFactory7> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_buffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_uploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_outputBuffer;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

    constexpr static UINT64 DATA_COUNT = 1024;

    std::array<Data, DATA_COUNT> m_data;
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
