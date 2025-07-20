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

    GetClientRect(hwnd, &m_windowRect);

    createCommandResources();
    createSwapChain(hwnd);
    createFence();

    createVertexBuffers();

    // ray tracing resources
    createAS();
    createRaytracingPipelineState();
    createRaytracingResources();
    createColorBuffer();
    createShaderTable();
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
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        HRESULT hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        if (FAILED(hr))
        {
            std::cerr << "Failed to get back buffer from swap chain." << std::endl;
            return;
        }
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
            .pResource = m_raytracingOutput.Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE,
            .StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);
}

void D3DEngine::recordCommands(UINT frameIndex)
{
    std::array descHeaps = {m_descHeap.Get()};
    m_commandList->SetDescriptorHeaps(descHeaps.size(), descHeaps.data());

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {
        .RayGenerationShaderRecord = {
            .StartAddress = m_shaderTable->GetGPUVirtualAddress(),
            .SizeInBytes = m_shaderRecordSize
        },
        .MissShaderTable = {
            .StartAddress = m_shaderTable->GetGPUVirtualAddress() + m_shaderRecordSize,
            .SizeInBytes = m_shaderRecordSize,
            .StrideInBytes = m_shaderRecordSize
        },
        .HitGroupTable = {
            .StartAddress = m_shaderTable->GetGPUVirtualAddress() + 2 * m_shaderRecordSize,
            .SizeInBytes = m_shaderRecordSize * 4,
            .StrideInBytes = m_shaderRecordSize
        },
        .Width = static_cast<UINT>(m_windowRect.right - m_windowRect.left),
        .Height = static_cast<UINT>(m_windowRect.bottom - m_windowRect.top),
        .Depth = 1
    };

    m_commandList->SetComputeRootSignature(m_rootSignature.Get());
    m_commandList->SetPipelineState1(m_raytracingPipelineState.Get());

    m_commandList->DispatchRays(&dispatchDesc);

    std::array barriers = {
        D3D12_RESOURCE_BARRIER{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource = m_raytracingOutput.Get(),
                .Subresource = 0,
                .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                .StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE
            }
        },
        D3D12_RESOURCE_BARRIER{
            .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
            .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
            .Transition = {
                .pResource = m_backBuffers[frameIndex].Get(),
                .Subresource = 0,
                .StateBefore = D3D12_RESOURCE_STATE_PRESENT,
                .StateAfter = D3D12_RESOURCE_STATE_COPY_DEST
            }
        }
    };
    m_commandList->ResourceBarrier(barriers.size(), barriers.data());

    m_commandList->CopyResource(
        m_backBuffers[frameIndex].Get(),
        m_raytracingOutput.Get()
    );
}

void D3DEngine::endFrame(UINT frameIndex)
{
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = m_backBuffers[frameIndex].Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
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

void D3DEngine::createVertexBuffers()
{
    createBuffer(
        m_vertexBuffer.GetAddressOf(),
        sizeof(DirectX::XMFLOAT3) * m_vertices.size(),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    DirectX::XMFLOAT3 *vertexMap = nullptr;
    HRESULT hr = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map vertex buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_vertices, vertexMap);
    m_vertexBuffer->Unmap(0, nullptr);

    createBuffer(
        m_planeVertexBuffer.GetAddressOf(),
        sizeof(DirectX::XMFLOAT3) * m_planeVertices.size(),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    DirectX::XMFLOAT3 *planeVertexMap = nullptr;
    hr = m_planeVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&planeVertexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map plane vertex buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_planeVertices, planeVertexMap);
    m_planeVertexBuffer->Unmap(0, nullptr);

    createBuffer(
        m_planeIndexBuffer.GetAddressOf(),
        sizeof(uint32_t) * m_planeIndices.size(),
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    uint32_t *planeIndexMap = nullptr;
    hr = m_planeIndexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&planeIndexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map plane index buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_planeIndices, planeIndexMap);
    m_planeIndexBuffer->Unmap(0, nullptr);
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
                .StrideInBytes = sizeof(DirectX::XMFLOAT3)
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

    // plane + triangle blas
    std::array planeGeometryDescs = {
        D3D12_RAYTRACING_GEOMETRY_DESC{
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
            .Triangles = {
                .IndexFormat = DXGI_FORMAT_R32_UINT,
                .VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT,
                .IndexCount = static_cast<UINT>(m_planeIndices.size()),
                .VertexCount = static_cast<UINT>(m_planeVertices.size()),
                .IndexBuffer = m_planeIndexBuffer->GetGPUVirtualAddress(), // stride is defined at IndexFormat
                .VertexBuffer = {
                    .StartAddress = m_planeVertexBuffer->GetGPUVirtualAddress(),
                    .StrideInBytes = sizeof(DirectX::XMFLOAT3)
                },
            }
        },
        geometryDesc
    };

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS planeInputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE,
        .NumDescs = static_cast<UINT>(planeGeometryDescs.size()),
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
        .pGeometryDescs = planeGeometryDescs.data()
    };

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO planePrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&planeInputs, &planePrebuildInfo);

    Microsoft::WRL::ComPtr<ID3D12Resource> planeBlasScratch;
    createBuffer(
        planeBlasScratch.GetAddressOf(),
        planePrebuildInfo.ScratchDataSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    createBuffer(
        m_planeBlas.GetAddressOf(),
        planePrebuildInfo.ResultDataMaxSizeInBytes,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
    );

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC planeBlasDesc = {
        .DestAccelerationStructureData = m_planeBlas->GetGPUVirtualAddress(),
        .Inputs = planeInputs,
        .ScratchAccelerationStructureData = planeBlasScratch->GetGPUVirtualAddress(),
    };
    m_commandList->BuildRaytracingAccelerationStructure(
        &planeBlasDesc,
        0,
        nullptr
    );

    D3D12_RESOURCE_BARRIER planeBarrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .UAV = {
            .pResource = m_planeBlas.Get()
        }
    };
    m_commandList->ResourceBarrier(1, &planeBarrier);

    // tlas
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {
        .Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL,
        .Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE,
        .NumDescs = 3,
        .DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY,
    };
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlasPrebuildInfo = {};
    m_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasPrebuildInfo);

    Microsoft::WRL::ComPtr<ID3D12Resource> tlasScratch;
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
        m_instanceDescBuffer.GetAddressOf(),
        sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * 3,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );
    m_tlasSize = static_cast<uint32_t>(tlasPrebuildInfo.ResultDataMaxSizeInBytes);

    std::array matrices = {
        DirectX::XMMatrixIdentity(),
        DirectX::XMMatrixTranslation(-1.2f, 0.0f, 0.0f),
        DirectX::XMMatrixTranslation(1.2f, 0.0f, 0.0f)
    };
    D3D12_RAYTRACING_INSTANCE_DESC *instanceDesc = nullptr;
    HRESULT hr = m_instanceDescBuffer->Map(0, nullptr, reinterpret_cast<void**>(&instanceDesc));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map instance descriptor buffer." << std::endl;
        return;
    }

    DirectX::XMFLOAT3X4 transformMatrix;
    DirectX::XMStoreFloat3x4(&transformMatrix, matrices[0]);
    memcpy(instanceDesc[0].Transform, &transformMatrix, sizeof(transformMatrix));
    instanceDesc[0].InstanceID = 0;
    instanceDesc[0].InstanceMask = 0xFF;
    instanceDesc[0].InstanceContributionToHitGroupIndex = 0;
    instanceDesc[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    instanceDesc[0].AccelerationStructure = m_planeBlas->GetGPUVirtualAddress();
    for (size_t i = 1; i < matrices.size(); ++i)
    {
        DirectX::XMStoreFloat3x4(&transformMatrix, matrices[i]);
        memcpy(instanceDesc[i].Transform, &transformMatrix, sizeof(transformMatrix));
        instanceDesc[i].InstanceID = static_cast<UINT>(i);
        instanceDesc[i].InstanceMask = 0xFF;
        instanceDesc[i].InstanceContributionToHitGroupIndex = i + 1;
        instanceDesc[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        instanceDesc[i].AccelerationStructure = m_blas->GetGPUVirtualAddress();
    }
    m_instanceDescBuffer->Unmap(0, nullptr);

    tlasInputs.InstanceDescs = m_instanceDescBuffer->GetGPUVirtualAddress();
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {
        .DestAccelerationStructureData = m_tlas->GetGPUVirtualAddress(),
        .Inputs = tlasInputs,
        .ScratchAccelerationStructureData = tlasScratch->GetGPUVirtualAddress(),
    };

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

    m_tlas->SetName(L"Top Level Acceleration Structure");
    m_blas->SetName(L"Bottom Level Acceleration Structure");
}

void D3DEngine::createRaytracingPipelineState()
{
    std::array<D3D12_STATE_SUBOBJECT, 13> subobjects = {};

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
        },
        D3D12_EXPORT_DESC{
            .Name = PLANE_CLOSE_SHADER,
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

    D3D12_HIT_GROUP_DESC planeHitGroupDesc = {
        .HitGroupExport = PLANE_HIT_GROUP,
        .ClosestHitShaderImport = PLANE_CLOSE_SHADER,
    };
    subobjects[2] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,
        .pDesc = &planeHitGroupDesc
    };

    // shader config
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {
        .MaxPayloadSizeInBytes = sizeof(RayTracingPayload),
        .MaxAttributeSizeInBytes = sizeof(BuiltInTriangleIntersectionAttributes)
    };
    subobjects[3] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,
        .pDesc = &shaderConfig
    };

    std::array exportNames = {
        RAYGEN_SHADER,
        MISS_SHADER,
        CLOSE_SHADER,
        PLANE_CLOSE_SHADER,
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION subobjectToExportsAssociation = {
        .pSubobjectToAssociate = &subobjects[3],
        .NumExports = static_cast<UINT>(exportNames.size()),
        .pExports = exportNames.data()
    };
    subobjects[4] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &subobjectToExportsAssociation
    };

    // pipeline config
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {
        .MaxTraceRecursionDepth = 0
    };
    subobjects[5] = D3D12_STATE_SUBOBJECT{
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

    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create root signature." << std::endl;
        return;
    }

    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSignature = {
        .pGlobalRootSignature = m_rootSignature.Get()
    };
    subobjects[6] = D3D12_STATE_SUBOBJECT{
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
    subobjects[7] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
        .pDesc = &localRootSignature
    };

    std::array raygenShaderExports = {
        RAYGEN_SHADER
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION raygenAssociation = {
        .pSubobjectToAssociate = &subobjects[7],
        .NumExports = static_cast<UINT>(raygenShaderExports.size()),
        .pExports = raygenShaderExports.data()
    };
    subobjects[8] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &raygenAssociation
    };

    // local root signature (for hit shader)
    D3D12_ROOT_PARAMETER hitParam = {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
        .Descriptor = {
            .ShaderRegister = 0,
            .RegisterSpace = 0
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL
    };
    D3D12_ROOT_SIGNATURE_DESC hitRootSignatureDesc = {
        .NumParameters = 1,
        .pParameters = &hitParam,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
    };
    hr = D3D12SerializeRootSignature(
        &hitRootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize miss/hit local root signature: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> hitRootSignature;
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&hitRootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create miss/hit local root signature." << std::endl;
        return;
    }

    D3D12_LOCAL_ROOT_SIGNATURE hitLocalRootSignature = {
        .pLocalRootSignature = hitRootSignature.Get()
    };
    subobjects[9] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
        .pDesc = &hitLocalRootSignature
    };

    std::array hitShaderExports = {
        CLOSE_SHADER
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION missHitAssociation = {
        .pSubobjectToAssociate = &subobjects[9],
        .NumExports = static_cast<UINT>(hitShaderExports.size()),
        .pExports = hitShaderExports.data()
    };
    subobjects[10] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &missHitAssociation
    };

    // local root signature (for miss shader, plane hit group)
    D3D12_ROOT_SIGNATURE_DESC missRootSignatureDesc = {
        .NumParameters = 0,
        .pParameters = nullptr,
        .NumStaticSamplers = 0,
        .pStaticSamplers = nullptr,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE
    };
    hr = D3D12SerializeRootSignature(
        &missRootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize miss local root signature: " << (errorBlob ? static_cast<const char*>(errorBlob->GetBufferPointer()) : "Unknown error") << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> missRootSignature;
    hr = m_device->CreateRootSignature(
        0,
        signature->GetBufferPointer(),
        signature->GetBufferSize(),
        IID_PPV_ARGS(&missRootSignature)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create miss local root signature." << std::endl;
        return;
    }

    D3D12_LOCAL_ROOT_SIGNATURE missLocalRootSignature = {
        .pLocalRootSignature = missRootSignature.Get()
    };
    subobjects[11] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE,
        .pDesc = &missLocalRootSignature
    };

    std::array missShaderExports = {
        MISS_SHADER,
        PLANE_CLOSE_SHADER
    };
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION missAssociation = {
        .pSubobjectToAssociate = &subobjects[11],
        .NumExports = static_cast<UINT>(missShaderExports.size()),
        .pExports = missShaderExports.data()
    };
    subobjects[12] = D3D12_STATE_SUBOBJECT{
        .Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION,
        .pDesc = &missAssociation
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

// layout: | raygen shader | miss shader | hit group 0 / geom 0 | hit group 0 / geom 1 | hit group 1 | hit group 2 |
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
    m_shaderRecordSize = align(
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + sizeof(D3D12_GPU_DESCRIPTOR_HANDLE),
        D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT
    );
    UINT totalSize = m_shaderRecordSize * 6;

    createBuffer(
        m_shaderTable.GetAddressOf(),
        totalSize,
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
    UINT64 srvHandle = m_descHeap->GetGPUDescriptorHandleForHeapStart().ptr;
    memcpy(
        shaderTableData + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
        &srvHandle,
        sizeof(D3D12_GPU_DESCRIPTOR_HANDLE)
    );

    // miss
    memcpy(
        shaderTableData + m_shaderRecordSize,
        stateObjectProperties->GetShaderIdentifier(MISS_SHADER),
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
    );

    // plane hit group
    memcpy(
        shaderTableData + 2 * m_shaderRecordSize,
        stateObjectProperties->GetShaderIdentifier(PLANE_HIT_GROUP),
        D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
    );

    // hit group
    for (UINT i = 0; i < 3; ++i)
    {
        memcpy(
            shaderTableData + (3 + i) * m_shaderRecordSize,
            stateObjectProperties->GetShaderIdentifier(HIT_GROUP),
            D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES
        );
        D3D12_GPU_VIRTUAL_ADDRESS cbvHandle = m_colorBuffers[i]->GetGPUVirtualAddress();
        memcpy(
            shaderTableData + (3 + i) * m_shaderRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
            &cbvHandle,
            sizeof(D3D12_GPU_VIRTUAL_ADDRESS)
        );
    }

    m_shaderTable->Unmap(0, nullptr);

}

void D3DEngine::createRaytracingResources()
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
        .Width = static_cast<UINT64>(m_windowRect.right - m_windowRect.left),
        .Height = static_cast<UINT>(m_windowRect.bottom - m_windowRect.top),
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
        D3D12_RESOURCE_STATE_COPY_SOURCE,
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
        .ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .RaytracingAccelerationStructure = {
            .Location = m_tlas->GetGPUVirtualAddress(),
        }
    };
    m_device->CreateShaderResourceView(
        nullptr,
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

void D3DEngine::createColorBuffer()
{
    D3D12_HEAP_PROPERTIES heapProperties = {
        .Type = D3D12_HEAP_TYPE_UPLOAD,
        .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
        .CreationNodeMask = 0,
        .VisibleNodeMask = 0
    };
    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = sizeof(DirectX::XMFLOAT4),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    for (UINT i = 0; i < m_colors.size(); ++i)
    {
        HRESULT hr = m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_colorBuffers[i])
        );
        if (FAILED(hr))
        {
            std::cerr << "Failed to create color buffer resource." << std::endl;
            return;
        }

        DirectX::XMFLOAT4 *colorMap = nullptr;
        hr = m_colorBuffers[i]->Map(0, nullptr, reinterpret_cast<void**>(&colorMap));
        if (FAILED(hr))
        {
            std::cerr << "Failed to map color buffer." << std::endl;
            return;
        }
        memcpy(colorMap, &m_colors[i], sizeof(DirectX::XMFLOAT4));
        m_colorBuffers[i]->Unmap(0, nullptr);
    }
}
