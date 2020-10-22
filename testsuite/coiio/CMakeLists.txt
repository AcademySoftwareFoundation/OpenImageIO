project(coiiotest)
cmake_minimum_required(VERSION 3.12)

find_package(OpenImageIO REQUIRED)

add_executable(coiiotest coiiotest.cpp)
target_link_libraries(coiiotest OpenImageIO::coiio)