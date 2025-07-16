#include "D3DEngine.h"

#include <array>
#include <iostream>

#include <d3dcompiler.h>

D3DEngine::D3DEngine()
{
#ifdef DEBUG
    enableDebugLayer();
#endif

    createDXGIFactory();
    createDevice();
    createCommandResources();
    createDescriptorHeap();
    createResources();
    createPipeline();
}

D3DEngine::~D3DEngine() = default;

void D3DEngine::run()
{
    std::cout << "D3DEngine is running..." << std::endl;
}

void D3DEngine::enableDebugLayer()
{
    Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
    HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
    if (hr == S_OK)
    {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(true);
    }
    else
    {
        std::cerr << "Failed to get D3D12 debug interface." << std::endl;
    }

    std::cout << "D3D12 debug layer enabled." << std::endl;
}

void D3DEngine::createDXGIFactory()
{
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr))
    {
        std::cerr << "Failed to create DXGI Factory." << std::endl;
        return;
    }
}

void D3DEngine::getAdapter(IDXGIAdapter1 **adapter)
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> localAdapter;

    for (
        UINT i = 0;
        m_dxgiFactory->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&localAdapter)
        ) != DXGI_ERROR_NOT_FOUND;
        ++i
    )
    {
        DXGI_ADAPTER_DESC1 desc;
        HRESULT hr = localAdapter->GetDesc1(&desc);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get adapter description." << std::endl;
            continue;
        }

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            continue;
        }

        break;
    }

    if (!localAdapter)
    {
        UINT adapterIndex = 0;
        SIZE_T memorySize = 0;

        for (
            UINT i = 0;
            m_dxgiFactory->EnumAdapters1(i, &localAdapter) != DXGI_ERROR_NOT_FOUND;
            ++i
        )
        {

            DXGI_ADAPTER_DESC1 desc;
            HRESULT hr = localAdapter->GetDesc1(&desc);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get adapter description." << std::endl;
                continue;
            }

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            if (desc.DedicatedVideoMemory > memorySize)
            {
                memorySize = desc.DedicatedVideoMemory;
                adapterIndex = i;
            }
        }

        HRESULT hr = m_dxgiFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&localAdapter)
        );
        if (FAILED(hr) && hr != DXGI_ERROR_NOT_FOUND)
        {
            std::cerr << "Failed to enumerate adapter by GPU preference." << std::endl;
            return;
        }
    }

    *adapter = localAdapter.Detach();
}

void D3DEngine::createDevice()
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    getAdapter(&adapter);

    std::array featureLevels = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    for (const auto& level : featureLevels)
    {
        HRESULT hr = D3D12CreateDevice(
            adapter.Get(),
            level,
            IID_PPV_ARGS(&m_device)
        );
        if (SUCCEEDED(hr))
        {
            return;
        }
    }

    if (!m_device)
    {
        std::cerr << "Failed to create D3D12 device." << std::endl;
    }
}

void D3DEngine::createCommandResources()
{
    HRESULT hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        IID_PPV_ARGS(&m_commandAllocator)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create command allocator." << std::endl;
        return;
    }

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COMPUTE,
        m_commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create command list." << std::endl;
        return;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr))
    {
        std::cerr << "Failed to create command queue." << std::endl;
        return;
    }
}

void D3DEngine::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 1,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to create descriptor heap." << std::endl;
        return;
    }
}

void D3DEngine::createResources()
{
    for (size_t i = 0; i < m_data.size(); ++i)
    {
        m_data[i].value = static_cast<float>(i);
        m_data[i].id = 0;
    }

    D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0,
    };
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(Data) * DATA_COUNT,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    };
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_buffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create buffer resource." << std::endl;
        return;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer = {
            .FirstElement = 0,
            .NumElements = DATA_COUNT,
            .StructureByteStride = sizeof(Data),
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE
        }
    };
    D3D12_CPU_DESCRIPTOR_HANDLE uavHandle = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_device->CreateUnorderedAccessView(
        m_buffer.Get(),
        nullptr,
        &uavDesc,
        uavHandle
    );

    D3D12_HEAP_PROPERTIES uploadHeapProperties = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0,
    };
    D3D12_RESOURCE_DESC uploadResourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(Data) * DATA_COUNT,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    hr = m_device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_uploadBuffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create upload buffer resource." << std::endl;
        return;
    }

    D3D12_HEAP_PROPERTIES outputHeapProperties = {
        .Type = D3D12_HEAP_TYPE_READBACK,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0,
    };
    D3D12_RESOURCE_DESC outputResourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(Data) * DATA_COUNT,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    hr = m_device->CreateCommittedResource(
        &outputHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &outputResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_outputBuffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create output buffer resource." << std::endl;
        return;
    }

    Data* uploadData = nullptr;
    hr = m_uploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&uploadData));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map upload buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_data, uploadData);
    m_uploadBuffer->Unmap(0, nullptr);

    m_commandList->CopyResource(m_buffer.Get(), m_uploadBuffer.Get());
}

void D3DEngine::createPipeline()
{
    D3D12_DESCRIPTOR_RANGE range = {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
    };
    D3D12_ROOT_PARAMETER parameter = {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        .DescriptorTable = {
            .NumDescriptorRanges = 1,
            .pDescriptorRanges = &range
        }
    };

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = 1,
        .pParameters = &parameter,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE
    };
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize root signature: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    hr = m_device->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create root signature." << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> computeShaderBlob;
    hr = D3DCompileFromFile(
        L"cs.hlsl",
        nullptr,
        nullptr,
        "main",
        "cs_5_1",
        0,
        0,
        &computeShaderBlob,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to compile compute shader: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineStateDesc = {
        .pRootSignature = m_rootSignature.Get(),
        .CS = {
            .pShaderBytecode = computeShaderBlob->GetBufferPointer(),
            .BytecodeLength = computeShaderBlob->GetBufferSize()
        },
        .NodeMask = 0,
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
    };

    hr = m_device->CreateComputePipelineState(
        &pipelineStateDesc,
        IID_PPV_ARGS(&m_pipelineState)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create compute pipeline state." << std::endl;
        return;
    }
}
