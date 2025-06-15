cmake_minimum_required(VERSION 3.28)
project(d2d_01_hello_world)

set(CMAKE_CXX_STANDARD 20)

add_executable(d2d_01_hello_world
        main.cpp
        Application.cpp
)
target_link_libraries(d2d_01_hello_world PRIVATE d2d1)
