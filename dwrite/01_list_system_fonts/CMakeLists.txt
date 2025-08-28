cmake_minimum_required(VERSION 3.28)
project(dwrite_01_list_system_fonts)

set(CMAKE_CXX_STANDARD 20)

add_executable(dwrite_01_list_system_fonts main.cpp Application.cpp)
target_link_libraries(dwrite_01_list_system_fonts PRIVATE d2d1 dwrite)