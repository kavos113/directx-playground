#ifndef D3D_08_CUSTOM_DEBUG_DEBUG_H
#define D3D_08_CUSTOM_DEBUG_DEBUG_H

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>

#include <map>
#include <string>

class Debug
{
public:
    static void enableDebugLayer();
    void setupCallback(const Microsoft::WRL::ComPtr<ID3D12Device>& device);
    void cleanup(const Microsoft::WRL::ComPtr<ID3D12Device>& device) const;

private:
    static void CALLBACK debugCallback(
        D3D12_MESSAGE_CATEGORY Category,
        D3D12_MESSAGE_SEVERITY Severity,
        D3D12_MESSAGE_ID ID,
        LPCSTR pDescription,
        void* pContext
    );

    DWORD m_callbackCookie = 0;

    static std::map<D3D12_MESSAGE_CATEGORY, std::wstring> m_categoryMap;
    static std::map<D3D12_MESSAGE_SEVERITY, std::wstring> m_severityMap;
};



#endif //D3D_08_CUSTOM_DEBUG_DEBUG_H
