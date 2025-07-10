cmake_minimum_required(VERSION 3.28)
project(d3d_25_offscreen)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_25_offscreen
        main.cpp
        Application.cpp
        D3DEngine.cpp
        Debug.cpp
        Model.cpp
)
target_link_libraries(d3d_25_offscreen PRIVATE d3d12 dxgi d3dcompiler)
target_compile_definitions(d3d_25_offscreen PRIVATE DEBUG)
target_include_directories(d3d_25_offscreen PRIVATE ${THIRD_PARTY_DIR})

find_package(directxmath CONFIG REQUIRED)
target_link_libraries(d3d_25_offscreen PRIVATE Microsoft::DirectXMath)

find_package(directxtex CONFIG REQUIRED)
target_link_libraries(d3d_25_offscreen PRIVATE Microsoft::DirectXTex)

file(COPY shader.hlsl postprocess.hlsl DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${D3D_RESOURCES_DIR}/texture.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${D3D_RESOURCES_DIR}/models/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/)