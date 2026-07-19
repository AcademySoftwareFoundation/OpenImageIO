# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.18)
project (oiio-imagebufalgo-opencv
         LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

set (CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to prefer (17, 20, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

find_package (OpenImageIO CONFIG REQUIRED)
find_package (Imath CONFIG REQUIRED)
find_package (OpenCV REQUIRED)

# Special for OIIO testsuite when running in sanitize mode
if (DEFINED ENV{SANITIZE})
    add_compile_options (-fsanitize=$ENV{SANITIZE})
    add_link_options (-fsanitize=$ENV{SANITIZE})
endif ()

add_executable (opencv-roundtrip src/opencv-roundtrip.cpp)
target_include_directories (opencv-roundtrip PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries (opencv-roundtrip PRIVATE OpenImageIO::OpenImageIO Imath::Imath ${OpenCV_LIBS})
