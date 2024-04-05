# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.15)
project (consumer
         LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

message (STATUS "Building ${PROJECT_NAME} ${PROJECT_VERSION} - ${CMAKE_BUILD_TYPE}")

# Use C++17
set (CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to prefer (17, 20, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)


# Make sure we have dependencies we need
find_package (OpenImageIO CONFIG REQUIRED)

# Special for OIIO testsuite when running in sanitize mode
if (DEFINED ENV{SANITIZE})
    add_compile_options (-fsanitize=$ENV{SANITIZE})
    add_link_options (-fsanitize=$ENV{SANITIZE})
endif()


add_executable(consumer consumer.cpp)
target_link_libraries (consumer
                       PRIVATE OpenImageIO::OpenImageIO)
