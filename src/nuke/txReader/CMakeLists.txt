# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


add_library (txReader MODULE txReader.cpp)

target_include_directories (txReader PUBLIC ${NUKE_INCLUDES})
target_link_libraries (txReader PUBLIC ${NUKE_LIBRARIES} OpenImageIO)
install_targets (txReader)

if (WIN32)
    target_compile_definitions (txReader PRIVATE FN_OS_WINDOWS USE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_OPTIONS /wd4251 /W3 /O2 /MD /GL /GR /GF
        LINK_FLAGS "/machine:x64 /SUBSYSTEM:WINDOWS /dll")
elseif (APPLE)
    target_link_libraries (txReader PRIVATE GLEW)
    target_compile_definitions (txReader PRIVATE USE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX ""
        COMPILE_OPTIONS -arch x86_64
        LINK_FLAGS "-arch x86_64 -bundle -framework QuartzCore -framework IOKit -framework CoreFoundation -framework Carbon -framework ApplicationServices -framework OpenGL -framework AGL")
else ()
    target_compile_definitions (txReader PRIVATE USE_GLEW)
    set_target_properties (txReader PROPERTIES
        PREFIX "")
endif ()

# Ignore Nuke 10's use of deprecated auto_ptr
if (NOT MSVC)
    set_source_files_properties (txReader.cpp PROPERTIES
                                    COMPILE_OPTIONS -Wno-deprecated-declarations)
endif ()
