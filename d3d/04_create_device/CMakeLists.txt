cmake_minimum_required(VERSION 3.28)
project(d3d_04_create_device)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_04_create_device
        main.cpp
        Application.cpp
        D3DEngine.cpp
)
target_link_libraries(d3d_04_create_device PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_04_create_device PRIVATE DEBUG)