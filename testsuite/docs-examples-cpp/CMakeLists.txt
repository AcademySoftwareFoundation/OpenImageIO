# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/OpenImageIO/oiio

cmake_minimum_required (VERSION 3.15)
project (oiio-docs-examples
         LANGUAGES CXX)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

message (STATUS "Building ${PROJECT_NAME} ${PROJECT_VERSION} - ${CMAKE_BUILD_TYPE}")

# Use C++14
set (CMAKE_CXX_STANDARD 14 CACHE STRING "C++ standard to prefer (14, 17, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)


# Make sure we have dependencies we need
find_package (OpenImageIO CONFIG REQUIRED)

# Special for OIIO testsuite when running in sanitize mode
if (DEFINED ENV{SANITIZE})
    add_compile_options (-fsanitize=$ENV{SANITIZE})
    add_link_options (-fsanitize=$ENV{SANITIZE})
endif()

add_executable(docs-examples-imageoutput src/docs-examples-imageoutput.cpp)
target_link_libraries (docs-examples-imageoutput
                       PRIVATE OpenImageIO::OpenImageIO)

# Chapters we haven't done yet:
# add_executable(docs-imageinput src/docs-examples-imageinput.cpp)
# target_link_libraries (docs-examples-imageinput
#                        PRIVATE OpenImageIO::OpenImageIO)
# 
# add_executable(docs-imagebuf src/docs-examples-imagebuf.cpp)
# target_link_libraries (docs-examples-imagebuf
#                        PRIVATE OpenImageIO::OpenImageIO)
# 
# add_executable(docs-imagebufalgo src/docs-examples-imagebufalgo.cpp)
# target_link_libraries (docs-examples-imagebufalgo
#                        PRIVATE OpenImageIO::OpenImageIO)
