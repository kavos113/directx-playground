cmake_minimum_required(VERSION 3.28)
    project(d3d_06_swap_chain)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_06_swap_chain
        main.cpp
        Application.cpp
        D3DEngine.cpp
)
target_link_libraries(d3d_06_swap_chain PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_06_swap_chain PRIVATE DEBUG)