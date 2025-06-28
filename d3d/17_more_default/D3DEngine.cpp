#include "D3DEngine.h"

#include <DirectXTex.h>

#include <array>
#include <iostream>

#define AlignCBuffer(x) (((x) + 0xff) & ~0xff)

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

    createDescriptorHeap();

    createVertexBuffer();
    createIndexBuffer();
    createColorBuffer();

    loadTexture(L"texture.png");

    createPipelineState();
    createViewport(hwnd);
}

D3DEngine::~D3DEngine() = default;

void D3DEngine::cleanup()
{
    waitForFence(m_commandQueue);

    CloseHandle(m_fenceEvent);

    m_fence.Reset();

    m_commandList.Reset();
    m_copyCommandList.Reset();

    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_colorBuffer.Reset();
    m_texture.Reset();
    m_descHeap.Reset();

    m_rootSignature.Reset();
    m_pipelineState.Reset();

    m_rtvHeap.Reset();
    for (auto& buffer : m_backBuffers)
    {
        buffer.Reset();
    }
    m_swapchain.Reset();

    m_commandQueue.Reset();
    m_commandAllocator.Reset();
    m_copyCommandQueue.Reset();
    m_copyCommandAllocator.Reset();

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
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create command allocator." << std::endl;
        return;
    }

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
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

    hr = m_device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_COPY,
        IID_PPV_ARGS(&m_copyCommandAllocator)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create copy command allocator." << std::endl;
        return;
    }

    hr = m_device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_COPY,
        m_copyCommandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_copyCommandList)
    );
    if     (FAILED(hr))
    {
        std::cerr << "Failed to create copy command list." << std::endl;
        return;
    }

    D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {
        .Type = D3D12_COMMAND_LIST_TYPE_COPY,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0
    };
    hr = m_device->CreateCommandQueue(
        &copyQueueDesc,
        IID_PPV_ARGS(&m_copyCommandQueue)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create copy command queue." << std::endl;
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
    HRESULT hr = m_device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&m_fence)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create fence." << std::endl;
        return;
    }

    m_fenceValue = 0;

    m_fenceEvent = CreateEvent(
        nullptr,
        FALSE,
        FALSE,
        nullptr
    );
    if (!m_fenceEvent)
    {
        std::cerr << "Failed to create fence event." << std::endl;
        return;
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

void D3DEngine::recordCommands(UINT frameIndex) const
{
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->IASetIndexBuffer(&m_indexBufferView);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetDescriptorHeaps(1, m_descHeap.GetAddressOf());
    m_commandList->SetGraphicsRootDescriptorTable(
        0, // Root parameter index
        m_descHeap->GetGPUDescriptorHandleForHeapStart()
    );

    m_commandList->SetPipelineState(m_pipelineState.Get());

    m_commandList->DrawIndexedInstanced(m_indices.size(), 1, 0, 0, 0);
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

    HRESULT hr = m_commandList->Close();
    if (FAILED(hr))
    {
        std::cerr << "Failed to close command list." << std::endl;
        return;
    }

    std::array<ID3D12CommandList*, 1> commandLists = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

    waitForFence(m_commandQueue);

    hr = m_commandAllocator->Reset();
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset command allocator." << std::endl;
        return;
    }

    hr = m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset command list." << std::endl;
        return;
    }

    hr = m_swapchain->Present(1, 0);
    if (FAILED(hr))
    {
        std::cerr << "Failed to present swap chain." << std::endl;
        return;
    }
}

void D3DEngine::waitForFence(Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue)
{
    m_fenceValue++;
    UINT64 fenceValue = m_fenceValue;
    HRESULT hr = queue->Signal(m_fence.Get(), fenceValue);
    if (FAILED(hr))
    {
        std::cerr << "Failed to signal command queue." << std::endl;
        return;
    }

    if (m_fence->GetCompletedValue() < fenceValue)
    {
        hr = m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
        if (FAILED(hr))
        {
            std::cerr << "Failed to set event on fence completion." << std::endl;
            return;
        }
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void D3DEngine::createVertexBuffer()
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
        .Width = sizeof(Vertex) * m_vertices.size(),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_vertexBuffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create vertex buffer." << std::endl;
        return;
    }

    Vertex *vertexMap = nullptr;
    hr = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
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

void D3DEngine::createIndexBuffer()
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
        .Width = sizeof(unsigned short) * m_indices.size(),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_indexBuffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create index buffer." << std::endl;
        return;
    }

    unsigned short *indexMap = nullptr;
    hr = m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map index buffer." << std::endl;
        return;
    }
    std::ranges::copy(m_indices, indexMap);
    m_indexBuffer->Unmap(0, nullptr);

    m_indexBufferView = {
        .BufferLocation = m_indexBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(sizeof(unsigned short) * m_indices.size()),
        .Format = DXGI_FORMAT_R16_UINT
    };
}

Microsoft::WRL::ComPtr<ID3D10Blob> D3DEngine::compileShader(
    const wchar_t *fileName,
    const char *entryPoint,
    const char *target,
    UINT flags
)
{
    Microsoft::WRL::ComPtr<ID3D10Blob> shaderBlob;
    Microsoft::WRL::ComPtr<ID3D10Blob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        fileName,
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint,
        target,
        flags,
        0,
        &shaderBlob,
        &errorBlob
    );
    if (FAILED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            std::wcerr << L"Shader file not found: " << fileName << std::endl;
            return nullptr;
        }

        if (errorBlob)
        {
            std::string errorMessage(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            std::cerr << "Shader compilation failed: " << errorMessage << std::endl;
        }
        else
        {
            std::cerr << "Shader compilation failed with HRESULT: " << hr << std::endl;
        }
        return nullptr;
    }

    return shaderBlob;
}

void D3DEngine::createPipelineState()
{
    auto ps = compileShader(L"shader.hlsl", "ps_main", "ps_5_0");
    if (!ps)
    {
        std::cerr << "Failed to compile pixel shader." << std::endl;
        return;
    }

    auto vs = compileShader(L"shader.hlsl", "vs_main", "vs_5_0");
    if (!vs)
    {
        std::cerr << "Failed to compile vertex shader." << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D10Blob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3D10Blob> errorBlob;

    std::array descriptorRanges = {
        D3D12_DESCRIPTOR_RANGE{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        },
        D3D12_DESCRIPTOR_RANGE{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };
    D3D12_ROOT_PARAMETER rootParameter = {
        .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
        .DescriptorTable = {
            .NumDescriptorRanges = static_cast<UINT>(descriptorRanges.size()),
            .pDescriptorRanges = descriptorRanges.data()
        },
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    D3D12_STATIC_SAMPLER_DESC samplerDesc = {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias = 0.0f,
        .MaxAnisotropy = 1,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS,
        .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        .MinLOD = 0.0f,
        .MaxLOD = D3D12_FLOAT32_MAX,
        .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
    };

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {
        .NumParameters = 1,
        .pParameters = &rootParameter,
        .NumStaticSamplers = 1,
        .pStaticSamplers = &samplerDesc,
        .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    };
    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to serialize root signature." << std::endl;
        if (errorBlob)
        {
            std::string errorMessage(static_cast<const char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize());
            std::cerr << "Error: " << errorMessage << std::endl;
        }
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


    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineDesc = {
        .pRootSignature = m_rootSignature.Get(),
        .VS = {
            .pShaderBytecode = vs->GetBufferPointer(),
            .BytecodeLength = vs->GetBufferSize()
        },
        .PS = {
            .pShaderBytecode = ps->GetBufferPointer(),
            .BytecodeLength = ps->GetBufferSize()
        },
        .BlendState = {
            .AlphaToCoverageEnable = FALSE,
            .IndependentBlendEnable = FALSE,
            .RenderTarget = {
                {
                    .BlendEnable = FALSE,
                    .LogicOpEnable = FALSE,
                    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
                }
            }
        },
        .SampleMask = D3D12_DEFAULT_SAMPLE_MASK,
        .RasterizerState = {
            .FillMode = D3D12_FILL_MODE_SOLID,
            .CullMode = D3D12_CULL_MODE_NONE,
            .FrontCounterClockwise = FALSE,
            .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
            .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
            .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
            .DepthClipEnable = TRUE,
            .MultisampleEnable = FALSE,
            .AntialiasedLineEnable = FALSE,
            .ForcedSampleCount = 0,
            .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
        },
        .InputLayout = {
            .pInputElementDescs = Vertex::inputLayout().data(),
            .NumElements = static_cast<UINT>(Vertex::inputLayout().size())
        },
        .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        .NumRenderTargets = 1,
        .RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        .DSVFormat = DXGI_FORMAT_D32_FLOAT,
        .SampleDesc = { 1, 0 },
        .NodeMask = 0,
        .Flags = D3D12_PIPELINE_STATE_FLAG_NONE
    };
    hr = m_device->CreateGraphicsPipelineState(
        &graphicsPipelineDesc,
        IID_PPV_ARGS(&m_pipelineState)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create graphics pipeline state." << std::endl;
        return;
    }
}

void D3DEngine::createViewport(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    m_viewport = {
        .TopLeftX = static_cast<FLOAT>(rc.left),
        .TopLeftY = static_cast<FLOAT>(rc.top),
        .Width = static_cast<FLOAT>(rc.right - rc.left),
        .Height = static_cast<FLOAT>(rc.bottom - rc.top),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };

    m_scissorRect = {
        .left = rc.left,
        .top = rc.top,
        .right = rc.right,
        .bottom = rc.bottom
    };
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
        .Width = AlignCBuffer(sizeof(float) * m_color.size()),
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_colorBuffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create color buffer." << std::endl;
        return;
    }

    float *colorMap = nullptr;
    hr = m_colorBuffer->Map(0, nullptr, reinterpret_cast<void**>(&colorMap));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map color buffer." << std::endl;
        return;
    }

    std::ranges::copy(m_color, colorMap);
    m_colorBuffer->Unmap(0, nullptr);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {
        .BufferLocation = m_colorBuffer->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(m_colorBuffer->GetDesc().Width) // Must be a multiple of 256 bytes
    };

    m_device->CreateConstantBufferView(
        &cbvDesc,
        m_descHeap->GetCPUDescriptorHandleForHeapStart()
    );
}

void D3DEngine::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 2,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0
    };
    HRESULT hr = m_device->CreateDescriptorHeap(
        &heapDesc,
        IID_PPV_ARGS(&m_descHeap)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create descriptor heap." << std::endl;
        return;
    }
}

void D3DEngine::loadTexture(const wchar_t *fileName)
{
    DirectX::TexMetadata metadata;
    DirectX::ScratchImage scratchImage;

    HRESULT hr = DirectX::LoadFromWICFile(
        fileName,
        DirectX::WIC_FLAGS_NONE,
        &metadata,
        scratchImage
    );
    if (FAILED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
        {
            std::wcerr << L"Texture file not found: " << fileName << std::endl;
        }
        else
        {
            std::wcerr << L"Failed to load texture from file: " << fileName << L" with error: " << std::hex << hr << std::endl;
        }
        return;
    }

    const DirectX::Image *image = scratchImage.GetImage(0, 0, 0);

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
        .Width = metadata.width,
        .Height = static_cast<UINT>(metadata.height),
        .DepthOrArraySize = static_cast<UINT16>(metadata.arraySize),
        .MipLevels = static_cast<UINT16>(metadata.mipLevels),
        .Format = metadata.format,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_texture)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create texture resource." << std::endl;
        return;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> stagingResource;
    createBuffer(
        AlignCBuffer(image->rowPitch) * image->height,
        stagingResource,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_GENERIC_READ
    );

    uint8_t *mappedData = nullptr;
    hr = stagingResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
    if (FAILED(hr))
    {
        std::cerr << "Failed to map staging resource for texture upload." << std::endl;
        return;
    }

    for (UINT y = 0; y < metadata.height; ++y)
    {
        std::memcpy(
            mappedData + y * image->rowPitch,
            image->pixels + y * image->rowPitch,
            image->rowPitch
        );
    }
    stagingResource->Unmap(0, nullptr);

    copyBuffer(stagingResource, m_texture);

    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = m_texture.Get(),
            .Subresource = 0,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {
        .Format = metadata.format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D = {
            .MostDetailedMip = 0,
            .MipLevels = static_cast<UINT>(metadata.mipLevels),
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.0f
        }
    };

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_descHeap->GetCPUDescriptorHandleForHeapStart();
    srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_device->CreateShaderResourceView(
        m_texture.Get(),
        &srvDesc,
        srvHandle
    );
}

// unsupported D3D12_HEAP_TYPE_CUSTOM
// create simple buffer(not texture)
void D3DEngine::createBuffer(
    UINT64 size,
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer,
    D3D12_HEAP_TYPE heapType,
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

    D3D12_TEXTURE_LAYOUT layout = heapType == D3D12_HEAP_TYPE_DEFAULT
        ? D3D12_TEXTURE_LAYOUT_UNKNOWN
        : D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = size,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {1, 0},
        .Layout = layout,
        .Flags = D3D12_RESOURCE_FLAG_NONE
    };
    HRESULT hr = m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create buffer resource." << std::endl;
        return;
    }
}

void D3DEngine::copyBuffer(
    Microsoft::WRL::ComPtr<ID3D12Resource> srcBuffer,
    Microsoft::WRL::ComPtr<ID3D12Resource> dstBuffer,
)
{
    D3D12_RESOURCE_DESC resourceDesc = dstBuffer->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
    UINT64 requiredSize = 0;
    m_device->GetCopyableFootprints(
        &resourceDesc,
        0,
        1,
        0,
        &layout,
        nullptr,
        nullptr,
        &requiredSize
    );

    D3D12_TEXTURE_COPY_LOCATION srcLocation = {
        .pResource = srcBuffer.Get(),
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = layout
    };

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {
        .pResource = dstBuffer.Get(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0
    };

    m_copyCommandList->CopyTextureRegion(
        &dstLocation,
        0, 0, 0,
        &srcLocation,
        nullptr
    );

    HRESULT hr = m_copyCommandList->Close();
    if (FAILED(hr))
    {
        std::cerr << "Failed to close copy command list." << std::endl;
        return;
    }

    std::array<ID3D12CommandList*, 1> commandLists = { m_copyCommandList.Get() };
    m_copyCommandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

    waitForFence(m_copyCommandQueue);

    hr = m_copyCommandAllocator->Reset();
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset copy command allocator." << std::endl;
        return;
    }

    hr = m_copyCommandList->Reset(m_copyCommandAllocator.Get(), nullptr);
    if (FAILED(hr))
    {
        std::cerr << "Failed to reset copy command list." << std::endl;
        return;
    }
}
