#include "D3DEngine.h"

#include <iostream>

D3DEngine::D3DEngine(HWND hwnd)
{
#ifdef DEBUG
    enableDebugLayer();
#endif

    createDXGIFactory();
    getAdapter();
}

D3DEngine::~D3DEngine() = default;

void D3DEngine::cleanup()
{
}

void D3DEngine::render()
{
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

void D3DEngine::getAdapter()
{
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

    UINT adapterIndex = 0;

    for (
        UINT i = 0;
        m_dxgiFactory->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)
        ) != DXGI_ERROR_NOT_FOUND;
        ++i
    )
    {
        DXGI_ADAPTER_DESC1 desc;
        HRESULT hr = adapter->GetDesc1(&desc);
        if (FAILED(hr))
        {
            std::cerr << "Failed to get adapter description." << std::endl;
            continue;
        }

        std::wcout << L"Adapter " << i << L"\n"
                        << desc.Description << L"\n"
                        << L"Video Memory: " << desc.DedicatedVideoMemory / (1024 * 1024) << L" MB\n"
                        << L"System Memory: " << desc.DedicatedSystemMemory / (1024 * 1024) << L" MB\n"
                        << L"Shared Memory: " << desc.SharedSystemMemory / (1024 * 1024) << L" MB\n" << std::endl;
    }

    HRESULT hr = m_dxgiFactory->EnumAdapterByGpuPreference(
        adapterIndex,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&adapter)
    );
    if (FAILED(hr) && hr != DXGI_ERROR_NOT_FOUND)
    {
        std::cerr << "Failed to enumerate adapter by GPU preference." << std::endl;
        return;
    }

    if (!adapter)
    {
        SIZE_T memorySize = 0;

        for (
            UINT i = 0;
            m_dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
            ++i
        )
        {

            DXGI_ADAPTER_DESC1 desc;
            HRESULT hr = adapter->GetDesc1(&desc);
            if (FAILED(hr))
            {
                std::cerr << "Failed to get adapter description." << std::endl;
                continue;
            }

            std::wcout << L"Adapter " << i << L"\n"
                            << desc.Description << L"\n"
                            << L"Video Memory: " << desc.DedicatedVideoMemory / (1024 * 1024) << L" MB\n"
                            << L"System Memory: " << desc.DedicatedSystemMemory / (1024 * 1024) << L" MB\n"
                            << L"Shared Memory: " << desc.SharedSystemMemory / (1024 * 1024) << L" MB\n" << std::endl;

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

        hr = m_dxgiFactory->EnumAdapterByGpuPreference(
            adapterIndex,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter)
        );
        if (FAILED(hr) && hr != DXGI_ERROR_NOT_FOUND)
        {
            std::cerr << "Failed to enumerate adapter by GPU preference." << std::endl;
            return;
        }
    }
}
