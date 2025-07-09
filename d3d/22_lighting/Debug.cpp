#include "Debug.h"

#include <iostream>
#include <sstream>

std::map<D3D12_MESSAGE_CATEGORY, std::wstring> Debug::m_categoryMap = {
    {D3D12_MESSAGE_CATEGORY_APPLICATION_DEFINED, L"Application Defined"},
    {D3D12_MESSAGE_CATEGORY_MISCELLANEOUS, L"Miscellaneous"},
    {D3D12_MESSAGE_CATEGORY_INITIALIZATION, L"Initialization"},
    {D3D12_MESSAGE_CATEGORY_CLEANUP, L"Cleanup"},
    {D3D12_MESSAGE_CATEGORY_COMPILATION, L"Compilation"},
    {D3D12_MESSAGE_CATEGORY_STATE_CREATION, L"State Creation"},
    {D3D12_MESSAGE_CATEGORY_STATE_SETTING, L"State Setting"},
    {D3D12_MESSAGE_CATEGORY_STATE_GETTING, L"State Getting"},
    {D3D12_MESSAGE_CATEGORY_RESOURCE_MANIPULATION, L"Resource Manipulation"},
    {D3D12_MESSAGE_CATEGORY_EXECUTION, L"Execution"},
    {D3D12_MESSAGE_CATEGORY_SHADER, L"Shader"},
};
std::map<D3D12_MESSAGE_SEVERITY, std::wstring> Debug::m_severityMap = {
    {D3D12_MESSAGE_SEVERITY_CORRUPTION, L"[CORRUPTION]"},
    {D3D12_MESSAGE_SEVERITY_ERROR,      L"[   ERROR  ]"},
    {D3D12_MESSAGE_SEVERITY_WARNING,    L"[  WARNING ]"},
    {D3D12_MESSAGE_SEVERITY_INFO,       L"[   INFO   ]"},
    {D3D12_MESSAGE_SEVERITY_MESSAGE,    L"[  MESSAGE ]"},
};

void Debug::enableDebugLayer()
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

void Debug::setupCallback(const Microsoft::WRL::ComPtr<ID3D12Device>& device)
{
    Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
    if (FAILED(hr))
    {
        std::cerr << "Failed to query ID3D12InfoQueue1 interface." << std::endl;
        return;
    }

    hr = infoQueue->RegisterMessageCallback(
        &Debug::debugCallback,
        D3D12_MESSAGE_CALLBACK_FLAG_NONE,
        nullptr,
        &m_callbackCookie
    );
    if (FAILED(hr))
    {
        std::cerr << "Failed to register debug callback." << std::endl;
        return;
    }

    std::cout << "D3D12 debug callback registered." << std::endl;
}

void Debug::cleanup(const Microsoft::WRL::ComPtr<ID3D12Device>& device) const
{
    Microsoft::WRL::ComPtr<ID3D12DebugDevice> debugDevice;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&debugDevice));
    if (SUCCEEDED(hr))
    {
        debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
        debugDevice.Reset();
    }
    else
    {
        std::cerr << "Failed to query ID3D12DebugDevice interface for cleanup." << std::endl;
    }

    Microsoft::WRL::ComPtr<ID3D12InfoQueue1> infoQueue;
    hr = device->QueryInterface(IID_PPV_ARGS(&infoQueue));
    if (SUCCEEDED(hr))
    {
        infoQueue->ClearStoredMessages();
        hr = infoQueue->UnregisterMessageCallback(m_callbackCookie);
        if (FAILED(hr))
        {
            std::cerr << "Failed to unregister debug callback." << std::endl;
        }
        infoQueue.Reset();
    }
    else
    {
        std::cerr << "Failed to query ID3D12InfoQueue1 interface for cleanup." << std::endl;
    }
}

void Debug::debugCallback(
    D3D12_MESSAGE_CATEGORY Category,
    D3D12_MESSAGE_SEVERITY Severity,
    D3D12_MESSAGE_ID ID,
    LPCSTR pDescription,
    void *pContext
)
{
    std::wstringstream wss;

    wss << m_severityMap[Severity] << L" ("
        << m_categoryMap[Category] << L") "
        << pDescription << std::endl;

    switch (Severity)
    {
        case D3D12_MESSAGE_SEVERITY_CORRUPTION:
        case D3D12_MESSAGE_SEVERITY_ERROR:
            std::wcerr << wss.str();
            break;
        case D3D12_MESSAGE_SEVERITY_WARNING:
            std::wcerr << wss.str();
            break;
        case D3D12_MESSAGE_SEVERITY_INFO:
        case D3D12_MESSAGE_SEVERITY_MESSAGE:
            std::wcout << wss.str();
            break;
    }
}
