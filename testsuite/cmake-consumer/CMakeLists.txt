# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

cmake_minimum_required (VERSION 3.12)
project (consumer
         LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

message (STATUS "Building ${PROJECT_NAME} ${PROJECT_VERSION} - ${CMAKE_BUILD_TYPE}")

# Use C++11
set (CMAKE_CXX_STANDARD 11 CACHE STRING "C++ standard to prefer (11, 14, 17, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)


# Make sure we have dependencies we need
find_package (OpenImageIO CONFIG REQUIRED)


add_executable(consumer consumer.cpp)
target_link_libraries (consumer
                       PRIVATE OpenImageIO::OpenImageIO)
