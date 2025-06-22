cmake_minimum_required(VERSION 3.28)
    project(d3d_07_present)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_07_present
        main.cpp
        Application.cpp
        D3DEngine.cpp
)
target_link_libraries(d3d_07_present PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_07_present PRIVATE DEBUG)