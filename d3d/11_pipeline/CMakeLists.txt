cmake_minimum_required(VERSION 3.28)
project(d3d_11_pipeline)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_11_pipeline
        main.cpp
        Application.cpp
        D3DEngine.cpp
        Debug.cpp
)
target_link_libraries(d3d_11_pipeline PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_11_pipeline PRIVATE DEBUG)

find_package(directxmath CONFIG REQUIRED)
target_link_libraries(d3d_11_pipeline PRIVATE Microsoft::DirectXMath)

file(COPY shader.hlsl DESTINATION ${CMAKE_CURRENT_BINARY_DIR})