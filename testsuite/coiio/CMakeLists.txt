cmake_minimum_required(VERSION 3.12)
project(coiiotest C)

find_package(OpenImageIO CONFIG REQUIRED)

add_executable(coiiotest coiiotest.c)
target_link_libraries(coiiotest OpenImageIO::OpenImageIO-C)
set_property(TARGET coiiotest PROPERTY C_STANDARD 99)
