cmake_minimum_required(VERSION 3.28)
project(d3d_09_vertex_buffer)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_09_vertex_buffer
        main.cpp
        Application.cpp
        D3DEngine.cpp
        Debug.cpp
)
target_link_libraries(d3d_09_vertex_buffer PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_09_vertex_buffer PRIVATE DEBUG)

find_package(directxmath CONFIG REQUIRED)
target_link_libraries(d3d_09_vertex_buffer PRIVATE Microsoft::DirectXMath)