cmake_minimum_required(VERSION 3.28)
project(d3d_19_depth_resource)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_19_depth_resource
        main.cpp
        Application.cpp
        D3DEngine.cpp
        Debug.cpp
)
target_link_libraries(d3d_19_depth_resource PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_19_depth_resource PRIVATE DEBUG)

find_package(directxmath CONFIG REQUIRED)
target_link_libraries(d3d_19_depth_resource PRIVATE Microsoft::DirectXMath)

find_package(directxtex CONFIG REQUIRED)
target_link_libraries(d3d_19_depth_resource PRIVATE Microsoft::DirectXTex)

file(COPY shader.hlsl DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${D3D_RESOURCES_DIR}/texture.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})