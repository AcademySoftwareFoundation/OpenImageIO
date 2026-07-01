# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.15)
project (oiio-openexr-partialtile
         LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

set (CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to prefer (17, 20, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

find_package (OpenImageIO CONFIG REQUIRED)

# Special for OIIO testsuite when running in sanitize mode
if (DEFINED ENV{SANITIZE})
    add_compile_options (-fsanitize=$ENV{SANITIZE})
    add_link_options (-fsanitize=$ENV{SANITIZE})
endif ()

add_executable (read-partial-tiles src/read-partial-tiles.cpp)
target_link_libraries (read-partial-tiles PRIVATE OpenImageIO::OpenImageIO)
