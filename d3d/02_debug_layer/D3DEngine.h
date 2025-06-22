#ifndef D3D_01_HELLO_WORLD_D3DENGINE_H
#define D3D_01_HELLO_WORLD_D3DENGINE_H

#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>

#include <d3d12.h>
#include <wrl/client.h>

class D3DEngine
{
public:
    explicit D3DEngine(HWND hwnd);
    ~D3DEngine();

    void cleanup();

    void render();

private:
    static void enableDebugLayer();
};



#endif //D3D_01_HELLO_WORLD_D3DENGINE_H
