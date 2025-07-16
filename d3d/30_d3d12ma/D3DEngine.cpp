#include "D3DEngine.h"

#include <DirectXTex.h>

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

    createDescriptorHeap();

    RECT rc;
    GetClientRect(hwnd, &rc);

    m_model = std::make_unique<Model>(
        m_device,
        m_descHeap,
        m_allocator,
        rc
    );

    createPipelineState();
    createViewport(hwnd);

    m_model->executeBarrier(m_commandList);
    executeCommand(0);
}

D3DEngine::~D3DEngine() = default;

void D3DEngine::cleanup()
{
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        waitForFence(m_commandQueue, i);
    }

    for (const auto & event : m_fenceEvents)
    {
        CloseHandle(event);
    }

    for (auto & fence : m_fence)
    {
        fence.Reset();
    }

    m_model->cleanup();

    m_commandList.Reset();
    m_descHeap.Reset();

    m_rootSignature.Reset();
    m_pipelineState.Reset();

    m_dsvHeap.Reset();
    for (auto& depthBuffer : m_depthBuffers)
    {
        depthBuffer.Reset();
    }

    m_rtvHeap.Reset();
    for (auto& buffer : m_backBuffers)
    {
        buffer.Reset();
    }
    m_swapchain.Reset();

    m_commandQueue.Reset();
    for (auto& allocator : m_commandAllocators)
    {
        allocator.Reset();
    }

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

void D3DEngine::getAdapter(IDXGIAdapter1 **adapter) const
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
            D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
            allocatorDesc.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;
            allocatorDesc.pDevice = m_device.Get();
            allocatorDesc.pAdapter = adapter.Get();
            HRESULT hr = D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator);
            if (FAILED(hr))
            {
                std::cerr << "Failed to create D3D12MA allocator." << std::endl;
                return;
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
    createDepthResources(rc.right - rc.left, rc.bottom - rc.top);
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
            0,
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
    HRESULT hr = m_commandAllocators[frameIndex]->Reset();
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
    auto dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    dsvHandle.ptr += frameIndex * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    m_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, &dsvHandle);
    m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor.data(), 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

}

void D3DEngine::recordCommands(UINT frameIndex) const
{
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetDescriptorHeaps(1, m_descHeap.GetAddressOf());

    auto gpuHandle = m_descHeap->GetGPUDescriptorHandleForHeapStart();
    m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);
    gpuHandle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

    m_commandList->SetPipelineState(m_pipelineState.Get());

    m_model->render(m_commandList);
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

    waitForFence(m_commandQueue, frameIndex);

    hr = m_swapchain->Present(1, 0);
    if (FAILED(hr))
    {
        std::cerr << "Failed to present swap chain." << std::endl;
        return;
    }
}

void D3DEngine::waitForFence(const Microsoft::WRL::ComPtr<ID3D12CommandQueue>& queue, UINT frameIndex)
{
    m_fenceValues[frameIndex]++;
    UINT64 fenceValue = m_fenceValues[frameIndex];
    HRESULT hr = queue->Signal(m_fence[frameIndex].Get(), fenceValue);
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

    D3D12_DESCRIPTOR_RANGE vsRange = {
        .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
        .NumDescriptors = 1,
        .BaseShaderRegister = 0,
        .RegisterSpace = 0,
        .OffsetInDescriptorsFromTableStart = 0
    };
    std::array psRanges = {
        D3D12_DESCRIPTOR_RANGE{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 1,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = 0
        },
        D3D12_DESCRIPTOR_RANGE{
            .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
        }
    };

    std::array rootParameters = {
        D3D12_ROOT_PARAMETER{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {
                .NumDescriptorRanges = 1,
                .pDescriptorRanges = &vsRange
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX
        },
        D3D12_ROOT_PARAMETER{
            .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
            .DescriptorTable = {
                .NumDescriptorRanges = static_cast<UINT>(psRanges.size()),
                .pDescriptorRanges = psRanges.data()
            },
            .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL
        }
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
        .NumParameters = static_cast<UINT>(rootParameters.size()),
        .pParameters = rootParameters.data(),
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
        .DepthStencilState = {
            .DepthEnable = TRUE,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = FALSE,
        },
        .InputLayout = {
            .pInputElementDescs = Model::Vertex::inputLayout().data(),
            .NumElements = static_cast<UINT>(Model::Vertex::inputLayout().size())
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

void D3DEngine::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 3,
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

void D3DEngine::barrier(
    const Microsoft::WRL::ComPtr<ID3D12Resource> &resource,
    D3D12_RESOURCE_STATES beforeState,
    D3D12_RESOURCE_STATES afterState
) const
{
    D3D12_RESOURCE_BARRIER barrier = {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition = {
            .pResource = resource.Get(),
            .Subresource = 0,
            .StateBefore = beforeState,
            .StateAfter = afterState
        }
    };
    m_commandList->ResourceBarrier(1, &barrier);
}

void D3DEngine::createDepthResources(UINT width, UINT height)
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = FRAME_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        .NodeMask = 0
    };
    HRESULT hr = m_device->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&m_dsvHeap)
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to create DSV descriptor heap." << std::endl;
        return;
    }

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc = {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = width,
        .Height = height,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_D32_FLOAT,
        .SampleDesc = {1, 0},
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    };

    D3D12_CLEAR_VALUE clearValue = {
        .Format = DXGI_FORMAT_D32_FLOAT,
        .DepthStencil = {
            .Depth = 1.0f,
            .Stencil = 0
        }
    };

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        hr = m_allocator->CreateResource(
            &allocDesc,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            &m_depthBuffers[i],
            IID_NULL,
            nullptr
        );
        if (FAILED(hr))
        {
            std::cerr << "Failed to create depth buffer resource." << std::endl;
            return;
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {
            .Format = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE
        };
        auto dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
        dsvHandle.ptr += i * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        m_device->CreateDepthStencilView(
            m_depthBuffers[i]->GetResource(),
            &dsvDesc,
            dsvHandle
        );
    }
}