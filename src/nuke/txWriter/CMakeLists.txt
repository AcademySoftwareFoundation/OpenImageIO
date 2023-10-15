# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

add_library (txWriter MODULE txWriter.cpp)

target_include_directories (txWriter PUBLIC ${NUKE_INCLUDES})
target_link_libraries (txWriter PUBLIC ${NUKE_LIBRARIES} OpenImageIO)
install_targets (txWriter)

if (WIN32)
    target_compile_definitions (txWriter PRIVATE FN_OS_WINDOWS USE_GLEW)
    set_target_properties (txWriter PROPERTIES
        PREFIX ""
        COMPILE_OPTIONS /wd4251 /W3 /O2 /MD /GL /GR /GF
        LINK_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /dll")
elseif (APPLE)
    target_link_libraries (txWriter PRIVATE GLEW)
    target_compile_definitions (txWriter PRIVATE USE_GLEW)
    set_target_properties (txWriter PROPERTIES
        PREFIX ""
        COMPILE_OPTIONS -arch x86_64
        LINK_FLAGS "-arch x86_64 -bundle -framework QuartzCore -framework IOKit -framework CoreFoundation -framework Carbon -framework ApplicationServices -framework OpenGL -framework AGL")
        # N.B. After CMake 3.13, use LINK_OPTIONS (list, not string)
else ()
    target_compile_definitions (txWriter PRIVATE USE_GLEW)
    set_target_properties (txWriter PROPERTIES
        PREFIX "")
endif ()

# Ignore Nuke 10's use of deprecated auto_ptr
if (NOT MSVC)
    set_source_files_properties (txWriter.cpp PROPERTIES
                                    COMPILE_OPTIONS -Wno-deprecated-declarations)
endif ()
