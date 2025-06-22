cmake_minimum_required(VERSION 3.30)
project(Win32D2DExamples)

add_subdirectory(win32/01_hello_world)
add_subdirectory(win32/02_split_class)

add_subdirectory(d2d/01_hello_world)
add_subdirectory(d2d/02_device_context)
add_subdirectory(d2d/03_draw_primitive_geometry)

add_subdirectory(d3d/01_hello_world)
add_subdirectory(d3d/02_debug_layer)
add_subdirectory(d3d/03_adapter)