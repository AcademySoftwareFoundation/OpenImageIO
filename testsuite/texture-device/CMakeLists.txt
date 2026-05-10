# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

cmake_minimum_required (VERSION 3.15)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    message (FATAL_ERROR
             "texture-device must be configured from OpenImageIO top-level CMake")
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE "Release")
endif ()

set (CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to prefer (17, 20, etc.)")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

if (TARGET OpenImageIO AND NOT TARGET OpenImageIO::OpenImageIO)
    add_library (OpenImageIO::OpenImageIO ALIAS OpenImageIO)
endif ()

if (NOT TARGET OpenImageIO::OpenImageIO)
    message (FATAL_ERROR
             "Missing target OpenImageIO::OpenImageIO; configure from OIIO root")
endif ()
if (NOT TARGET Imath::Imath)
    message (FATAL_ERROR
             "Missing target Imath::Imath; configure from OIIO root")
endif ()

# Special for OIIO testsuite when running in sanitize mode
if (DEFINED ENV{SANITIZE})
    add_compile_options (-fsanitize=$ENV{SANITIZE})
    add_link_options (-fsanitize=$ENV{SANITIZE})
endif()

add_executable (texture-device
    arena.cpp
    blend.cpp
    host.cpp
    texture_loader.cpp
    device_tests.cpp
)
target_link_libraries (texture-device
    PRIVATE OpenImageIO::OpenImageIO Imath::Imath)

if (MSVC)
    target_compile_options (texture-device PRIVATE /utf-8)
endif ()

# Compile+link gate for code that is intended to remain device-safe and
# header-only. This target must build without linking against OpenImageIO
# libraries, so unresolved symbols are detected at link time.
add_executable (texture-device-devicecheck
    arena.cpp
    blend.cpp
    device_tests.cpp
    devicecheck_main.cpp
)
if (MSVC)
    target_compile_options (texture-device-devicecheck PRIVATE /utf-8)
    # devicecheck intentionally does not link OIIO libs; avoid dllimport on
    # inline OIIO types (like ustringhash) to prevent unresolved imports.
    target_compile_definitions (texture-device-devicecheck PRIVATE OIIO_STATIC_DEFINE)
endif ()
target_include_directories (texture-device-devicecheck
    PRIVATE
        $<TARGET_PROPERTY:OpenImageIO::OpenImageIO,INTERFACE_INCLUDE_DIRECTORIES>
)

if (TARGET Imath::Imath)
    target_include_directories (texture-device-devicecheck
        PRIVATE
            $<TARGET_PROPERTY:Imath::Imath,INTERFACE_INCLUDE_DIRECTORIES>
    )
endif ()
