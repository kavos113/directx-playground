cmake_minimum_required(VERSION 3.28)
project(d2d_03_draw_primitive_geometry)

set(CMAKE_CXX_STANDARD 20)

add_executable(d2d_03_draw_primitive_geometry
        main.cpp
        Application.cpp
)
target_link_libraries(d2d_03_draw_primitive_geometry PRIVATE d2d1 d3d11 dxgi)