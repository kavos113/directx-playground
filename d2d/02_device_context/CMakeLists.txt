cmake_minimum_required(VERSION 3.28)
project(d2d_02_device_context)

set(CMAKE_CXX_STANDARD 20)

add_executable(d2d_02_device_context
        main.cpp
        Application.cpp
)
target_link_libraries(d2d_02_device_context PRIVATE d2d1 d3d11 dxgi)