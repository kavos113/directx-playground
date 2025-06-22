cmake_minimum_required(VERSION 3.28)
project(d3d_01_hello_world)

set(CMAKE_CXX_STANDARD 20)

add_executable(d3d_01_hello_world
        main.cpp
        Application.cpp
        D3DEngine.cpp
)
