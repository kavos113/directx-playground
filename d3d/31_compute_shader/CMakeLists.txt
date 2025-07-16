cmake_minimum_required(VERSION 3.28)
project(d3d_31_compute_shader)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_31_compute_shader
        main.cpp
        D3DEngine.cpp
)
target_link_libraries(d3d_31_compute_shader PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_31_compute_shader PRIVATE DEBUG)

file(COPY cs.hlsl DESTINATION ${CMAKE_CURRENT_BINARY_DIR})