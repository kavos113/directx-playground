#include "D3DEngine.h"

#include <dxcapi.h>

#include <array>
#include <iostream>

D3DEngine::D3DEngine(HWND hwnd)
{
#ifdef DEBUG
    m_debug = std::make_unique<Debug>();
    Debug::enableDebugLayer();
#endif

    createDXGIFactory();
    createDevice();
    m_debug->setupCallback(m_device);

    createCommandResources();
    createSwapChain(hwnd);
    createFence();

    createVertexBuffer();

    // ray tracing resources
    createAS();
    createRaytracingPipelineState();
    createShaderTable();
    RECT rc;
    GetClientRect(hwnd, &rc);
    createRaytracingResources(
        static_cast<UINT>(rc.right - rc.left),
        static_cast<UINT>(rc.bottom - rc.top)
    );
}

D3DEngine::~D3DEngine() = default;

void D3DEngine::cleanup()
{
    for (int i = 0; i < FRAME_COUNT; ++i)
    {
        waitForFence(i);
    }

    for (auto & event : m_fenceEvents)
    {
        if (event)
        {
            CloseHandle(event);
            event = nullptr;
        }
    }

    for (auto& commandAllocator : m_commandAllocators)
    {
        commandAllocator.Reset();
    }

    for (auto & fence : m_fence)
    {
        fence.Reset();
    }

    m_commandList.Reset();

    m_rtvHeap.Reset();
    for (auto& buffer : m_backBuffers)
    {
        buffer.Reset();
    }
    m_swapchain.Reset();

    m_commandQueue.Reset();

    m_debug->cleanup(m_device);
    m_debug.reset();

    m_device.Reset();
    m_dxgiFactory.Reset();
}

void D3DEngine::render()
{
    UINT frameIndex = m_swapchain->GetCurrentBackBufferIndex();

    beginFrame(frameIndex);
    recordCommands(frameIndex);
    endFrame(frameIndex);
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
            D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
            hr = m_device->CheckFeatureSupport(
                D3D12_FEATURE_D3D12_OPTIONS5,
                &options5,
                sizeof(options5)
            );
            if (SUCCEEDED(hr) && options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
            {
                std::cout << "Ray tracing: Supported." << std::endl;
            }
            else
            {
                std::cerr << "Ray tracing: Not Supported." << std::endl;
                continue;
            }

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
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_commandAllocators[i])
        );
        if (FAILED(hr))
        {
            std::cerr << "Failed to create command allocator." << std::endl;
            return;
        }
    }

    HRESULT hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create command list." << std::endl;
        return;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };
    hr = m_device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&m_commandQueue)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create command queue." << std::endl;
        return;
    }
}

void D3DEngine::createSwapChain(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
        .Width = static_cast<UINT>(rc.right - rc.left),
        .Height = static_cast<UINT>(rc.bottom - rc.top),
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .Stereo = FALSE,
        .SampleDesc = {1, 0},
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = FRAME_COUNT,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
    };
    HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        reinterpret_cast<IDXGISwapChain1**>(m_swapchain.GetAddressOf())
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create swap chain." << std::endl;
        return;
    }

    createSwapChainResources();
}

void D3DEngine::createSwapChainResources()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = 2,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };
    HRESULT hr = m_device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(&m_rtvHeap)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create RTV descriptor heap." << std::endl;
        return;
    }

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
    hr = m_swapchain->GetDesc1(&swapChainDesc);
    if (FAILED(hr))
    {
        std::cerr << "Failed to get swap chain description." << std::endl;
        return;
    }

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        if (FAILED(hr))
        {
            std::cerr << "Failed to get back buffer from swap chain." << std::endl;
            return;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += i * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
    }
}

void D3DEngine::createFence()
{
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        m_fenceValues[i] = 0;
        m_fenceEvents[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!m_fenceEvents[i])
        {
            std::cerr << "Failed to create fence event." << std::endl;
            return;
        }

        HRESULT hr = m_device->CreateFence(
            m_fenceValues[i],
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_fence[i])
        );
        if (FAILED(hr))
        {
            std::cerr << "Failed to create fence." << std::endl;
            return;
        }
    }
}

void D3DEngine::beginFrame(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = m_backBuffers[frameIndex].Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
            .StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    auto rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += frameIndex * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    m_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);
    m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor.data(), 0, nullptr);

}

void D3DEngine::recordCommands(UINT frameIndex)
{
}

void D3DEngine::endFrame(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = m_backBuffers[frameIndex].Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET,
            .StateAfter = D3D12_RESOURCE_STATE_PRESENT
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    executeCommand(frameIndex);

    HRESULT hr = m_swapchain->Present(1, 0);
    if (FAILED(hr))
    {
        std::cerr << "Failed to present swap chain." << std::endl;
        return;
    }
}

void D3DEngine::executeCommand(UINT frameIndex)
{
    HRESULT hr = m_commandList->Close();
    if (FAILED(hr))
    {
        std::cerr << "Failed to close command list." << std::endl;
        return;
    }

    std::array<ID3D12CommandList*, 1> commandLists = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

    waitForFence(frameIndex);

    hr = m_commandAllocators[frameIndex]->Reset();
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset command allocator." << std::endl;
        return;
    }

    hr = m_commandList->Reset(m_commandAllocators[frameIndex].Get(), nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset command list." << std::endl;
        return;
    }
}

void D3DEngine::waitForFence(UINT frameIndex)
{
    m_fenceValues[frameIndex]++;
    UINT64 fenceValue = m_fenceValues[frameIndex];
    HRESULT hr = m_commandQueue->Signal(m_fence[frameIndex].Get(), fenceValue);
    if (FAILED(hr))
    {
        std::cerr << "Failed to signal command queue." << std::endl;
        return;
    }

    if (m_fence[frameIndex]->GetCompletedValue() < fenceValue)
    {
        hr = m_fence[frameIndex]->SetEventOnCompletion(fenceValue, m_fenceEvents[frameIndex]);
        if (FAILED(hr))
        {
            std::cerr << "Failed to set event on fence completion." << std::endl;
            return;
        }
        WaitForSingleObject(m_fenceEvents[frameIndex], INFINITE);
    }
}

void D3DEngine::createBuffer(
    ID3D12Resource** buffer,
    size_t size,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialState
)
{
    D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = heapType,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = flags
    };
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(buffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create buffer resource." << std::endl;
        return;
    }
}

void D3DEngine::createVertexBuffer()
{
    createBuffer(
        m_vertexBuffer.GetAddressOf(),
        sizeof(Vertex) * m_vertices.size(),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    Vertex *vertexMap = nullptr;
    HRESULT hr = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map vertex buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_vertices, vertexMap);
    m_vertexBuffer->Unmap(0, nullptr);

    m_vertexBufferView = {
        .BufferLocation = m_vertexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(sizeof(Vertex) * m_vertices.size()),
        .StrideInBytes = sizeof(Vertex)
    };
}

void D3DEngine::createAS()
{
    // blas
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {
        .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
        .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
        .Triangles = {
            .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
            .VertexCount = static_cast<UINT>(m_vertices.size()),
            .VertexBuffer = {
                .StartAddress = m_vertexBuffer->GetGPUVirtualAddress(),
                .StrideInBytes = sizeof(Vertex)
            },
        }
    };

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
        .NumDescs = 1,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = &geometryDesc
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

    Microsoft::WRL::ComPtr<ID3D12Resource> blasScratch;
    createBuffer(
        blasScratch.GetAddressOf(),
        prebuildInfo.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    createBuffer(
        m_blas.GetAddressOf(),
        prebuildInfo.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
    );

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {
        .DestAccelerationStructureData = m_blas->GetGPUVirtualAddress(),
        .Inputs = inputs,
        .ScratchAccelerationStructureData = blasScratch->GetGPUVirtualAddress(),
    };
    m_commandList->BuildRaytracingAccelerationStructure(
        &blasDesc,
        0,
        nullptr
    );

    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .UAV = {
            .pResource = m_blas.Get()
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    // tlas
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
        .NumDescs = 1,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
    };
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratch;
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceDescBuffer;
    createBuffer(
        tlasScratch.GetAddressOf(),
        tlasPrebuildInfo.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    createBuffer(
        m_tlas.GetAddressOf(),
        tlasPrebuildInfo.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
    );
    createBuffer(
        instanceDescBuffer.GetAddressOf(),
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    m_tlasSize = static_cast<uint32_t>(tlasPrebuildInfo.ResultDataMaxSizeInBytes);

    DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();
    D3D12_RAYTRACING_INSTANCE_DESC *instanceDesc = nullptr;
    HRESULT hr = instanceDescBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instanceDesc));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map instance descriptor buffer." << std::endl;
        return;
    }
    instanceDesc->InstanceID = 0;
    instanceDesc->InstanceMask = 0xFF;
    instanceDesc->InstanceContributionToHitGroupIndex = 0;
    instanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDesc->AccelerationStructure = m_blas->GetGPUVirtualAddress();
    DirectX::XMStoreFloat4x4(
        reinterpret_cast<DirectX::XMFLOAT4X4*>(&instanceDesc->Transform),
        DirectX::XMMatrixTranspose(transform)
    );
    instanceDescBuffer->Unmap(0, nullptr);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {
        .DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress(),
        .Inputs = tlasInputs,
        .ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress(),
    };
    tlasDesc.Inputs.InstanceDescs = instanceDescBuffer->GetGPUVirtualAddress();

    m_commandList->BuildRaytracingAccelerationStructure(
        &tlasDesc,
        0,
        nullptr
    );

    D3D12_RESOURCE_BARRIER tlasBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .UAV = {
            .pResource = m_tlas.Get()
        }
    };
    m_commandList->ResourceBarrier(1, &tlasBarrier);

    executeCommand(0);
}

void D3DEngine::createRaytracingPipelineState()
{
    std::array<D3D12_STATE_SUBOBJECT, 10> subobjects = {};

    // dxil library
    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler;
    Microsoft::WRL::ComPtr<IDxcUtils> utils;
    HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr))
    {
        std::cerr << "Failed to create DXC compiler instance." << std::endl;
        return;
    }
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
    if (FAILED(hr))
    {
        std::cerr << "Failed to create DXC utils instance." << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
    hr = utils->CreateDefaultIncludeHandler(&includeHandler);
    if (FAILED(hr))
    {
        std::cerr << "Failed to create default include handler." << std::endl;
        return;
    }

    std::array args = {
        SHADER_FILE,
        L"-T", L"lib_6_3",
    };

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
    hr = utils->LoadFile(SHADER_FILE, nullptr, &sourceBlob);
    if (FAILED(hr))
    {
        std::cerr << "Failed to load shader file." << std::endl;
        return;
    }

    DxcBuffer sourceBuffer = {
        .Ptr = sourceBlob->GetBufferPointer(),
        .Size = sourceBlob->GetBufferSize(),
        .Encoding = DXC_CP_ACP
    };

    Microsoft::WRL::ComPtr<IDxcResult> result;
    hr = compiler->Compile(
        &sourceBuffer,
        args.data(),
        args.size(),
        includeHandler.Get(),
        IID_PPV_ARGS(&result)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to compile shader." << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
    hr = result->GetOutput(
        DXC_OUT_ERRORS,
        IID_PPV_ARGS(&errors),
        nullptr
    );
    if (FAILED(hr) || (errors != nullptr && errors->GetStringLength() > 0))
    {
        std::cerr << "Shader compilation errors: " << (errors ? errors->GetStringPointer() : "None") << std::endl;
        return;
    }

    result->GetStatus(&hr);
    if (FAILED(hr))
    {
        std::cerr << "Shader compilation failed with error code: " << hr << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
    Microsoft::WRL::ComPtr<IDxcBlobUtf16> shaderName;
    hr = result->GetOutput(
        DXC_OUT_OBJECT,
        IID_PPV_ARGS(&shaderBlob),
        &shaderName
    );
    if (FAILED(hr) || !shaderBlob)
    {
        std::cerr << "Failed to get compiled shader object." << std::endl;
        return;
    }

    std::array exportDescs = {
        D3D12_EXPORT_DESC{
            .Name = RAYGEN_SHADER,
            .ExportToRename = nullptr,
            .Flags = D3D12_EXPORT_FLAG_NONE
        },
        D3D12_EXPORT_DESC{
            .Name = MISS_SHADER,
            .ExportToRename = nullptr,
            .Flags = D3D12_EXPORT_FLAG_NONE
        },
        D3D12_EXPORT_DESC{
            .Name = CLOSE_SHADER,
            .ExportToRename = nullptr,
            .Flags = D3D12_EXPORT_FLAG_NONE
        }
    };

    D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {
        .DXILLibrary = {
            .pShaderBytecode = shaderBlob->GetBufferPointer(),
            .BytecodeLength = shaderBlob->GetBufferSize(),
        },
        .NumExports = static_cast<UINT>(exportDescs.size()),
        .pExports = exportDescs.data()
    };

    subobjects[0] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,
        .pDesc = &dxilLibDesc
    };

    // hit group
    D3D12_HIT_GROUP_DESC hitGroupDesc = {
        .HitGroupExport = HIT_GROUP,
        .ClosestHitShaderImport = CLOSE_SHADER,
    };
    subobjects[1] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
        .pDesc = &hitGroupDesc
    };

    // shader config
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {
        .MaxPayloadSizeInBytes = sizeof(RayTracingPayload),
        .MaxAttributeSizeInBytes = sizeof(BuiltInTriangleIntersectionAttributes)
    };
    subobjects[2] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
        .pDesc = &shaderConfig
    };

    std::array exportNames = {
        RAYGEN_SHADER,
        MISS_SHADER,
        CLOSE_SHADER
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION subobjectToExportsAssociation = {
        .pSubobjectToAssociate = &subobjects[2],
        .NumExports = static_cast<UINT>(exportNames.size()),
        .pExports = exportNames.data()
    };
    subobjects[3] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &subobjectToExportsAssociation
    };

    // pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {
        .MaxTraceRecursionDepth = 0
    };
    subobjects[4] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,
        .pDesc = &pipelineConfig
    };

    // global root signature
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = 0,
        .pParameters = nullptr,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE
    };
    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize root signature: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create root signature." << std::endl;
        return;
    }

    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature = {
        .pGlobalRootSignature = rootSignature.Get()
    };
    subobjects[5] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,
        .pDesc = &globalRootSignature
    };

    // local root signature (for ray gen shader)
    std::array ranges = {
        D3D12_DESCRIPTOR_RANGE{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        D3D12_DESCRIPTOR_RANGE{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };
    D3D12_ROOT_PARAMETER param = {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = {
            .NumDescriptorRanges = static_cast<UINT>(ranges.size()),
            .pDescriptorRanges = ranges.data()
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
    };
    D3D12_ROOT_SIGNATURE_DESC raygenRootSignatureDesc = {
        .NumParameters = 1,
        .pParameters = &param,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
    };
    hr = D3D12SerializeRootSignature(
        &raygenRootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize local root signature: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> raygenRootSignature;
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&raygenRootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create local root signature." << std::endl;
        return;
    }

    D3D12_LOCAL_ROOT_SIGNATURE localRootSignature = {
        .pLocalRootSignature = raygenRootSignature.Get()
    };
    subobjects[6] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
        .pDesc = &localRootSignature
    };

    std::array raygenShaderExports = {
        RAYGEN_SHADER
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION raygenAssociation = {
        .pSubobjectToAssociate = &subobjects[6],
        .NumExports = static_cast<UINT>(raygenShaderExports.size()),
        .pExports = raygenShaderExports.data()
    };
    subobjects[7] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &raygenAssociation
    };

    // local root signature (for miss/hit shader)
    D3D12_ROOT_SIGNATURE_DESC missHitRootSignatureDesc = {
        .NumParameters = 0,
        .pParameters = nullptr,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
    };
    hr = D3D12SerializeRootSignature(
        &missHitRootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize miss/hit local root signature: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> missHitRootSignature;
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&missHitRootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create miss/hit local root signature." << std::endl;
        return;
    }

    D3D12_LOCAL_ROOT_SIGNATURE missHitLocalRootSignature = {
        .pLocalRootSignature = missHitRootSignature.Get()
    };
    subobjects[8] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
        .pDesc = &missHitLocalRootSignature
    };

    std::array missHitShaderExports = {
        MISS_SHADER,
        CLOSE_SHADER
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION missHitAssociation = {
        .pSubobjectToAssociate = &subobjects[8],
        .NumExports = static_cast<UINT>(missHitShaderExports.size()),
        .pExports = missHitShaderExports.data()
    };
    subobjects[9] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &missHitAssociation
    };

    D3D12_STATE_OBJECT_DESC stateObjectDesc = {
        .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
        .NumSubobjects = static_cast<UINT>(subobjects.size()),
        .pSubobjects = subobjects.data()
    };
    hr = m_device->CreateStateObject(
        &stateObjectDesc,
        IID_PPV_ARGS(&m_raytracingPipelineState)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create ray tracing pipeline state." << std::endl;
        return;
    }

}

UINT align(UINT value, UINT alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

// layout: | raygen shader | miss shader | hit group |
void D3DEngine::createShaderTable()
{
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
    HRESULT hr = m_raytracingPipelineState.As(&stateObjectProperties);
    if (FAILED(hr))
    {
        std::cerr << "Failed to get state object properties." << std::endl;
        return;
    }

    // must align the largest record size
    UINT recordSize = align(
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE),
        D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
    );
    UINT totalSize = recordSize * 3;

    createBuffer(
        m_shaderTable.GetAddressOf(),
        recordSize,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    uint8_t *shaderTableData = nullptr;
    hr = m_shaderTable->Map(0, nullptr, reinterpret_cast<void**>(&shaderTableData));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map shader table." << std::endl;
        return;
    }

    // raygen
    memcpy(
        shaderTableData,
        stateObjectProperties->GetShaderIdentifier(RAYGEN_SHADER),
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
    );

    // miss
    memcpy(
        shaderTableData + recordSize,
        stateObjectProperties->GetShaderIdentifier(MISS_SHADER),
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
    );

    // hit group
    memcpy(
        shaderTableData + 2 * recordSize,
        stateObjectProperties->GetShaderIdentifier(HIT_GROUP),
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
    );

    m_shaderTable->Unmap(0, nullptr);

}

void D3DEngine::createRaytracingResources(UINT width, UINT height)
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 2, // | tlas | output |
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };
    HRESULT hr = m_device->CreateDescriptorHeap(
        &srvHeapDesc,
        IID_PPV_ARGS(&m_descHeap)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create SRV descriptor heap." << std::endl;
        return;
    }

    D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = D3D12_HEAP_TYPE_DEFAULT,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    };
    hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        nullptr,
        IID_PPV_ARGS(&m_raytracingOutput)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create ray tracing output resource." << std::endl;
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_descHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .RaytracingAccelerationStructure.Location = m_tlas->GetGPUVirtualAddress()
    };
    m_device->CreateShaderResourceView(
        m_tlas.Get(),
        &srvDesc,
        srvHandle
    );

    srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
        .Texture2D = {
            .MipSlice = 0
        }
    };
    m_device->CreateUnorderedAccessView(
        m_raytracingOutput.Get(),
        nullptr,
        &uavDesc,
        srvHandle
    );
}
