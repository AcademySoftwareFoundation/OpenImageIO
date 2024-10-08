# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

set (OIIO_IV_EXTRA_IV_LIBRARIES "" CACHE STRING "Paths to extra libraries to force iv to link against")

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTOMOC_PATH_PREFIX OFF)
if (Qt5_POSITION_INDEPENDENT_CODE OR Qt6_POSITION_INDEPENDENT_CODE)
    set (CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

check_is_enabled (iv iv_enabled)
if (iv_enabled AND (Qt5_FOUND OR Qt6_FOUND) AND OPENGL_FOUND)
    fancy_add_executable (
        SYSTEM_INCLUDE_DIRS
            ${OPENGL_INCLUDE_DIR}
        LINK_LIBRARIES
            OpenImageIO
            $<TARGET_NAME_IF_EXISTS:Qt5::Core>
            $<TARGET_NAME_IF_EXISTS:Qt5::Gui>
            $<TARGET_NAME_IF_EXISTS:Qt5::Widgets>
            $<TARGET_NAME_IF_EXISTS:Qt5::OpenGL>
            $<TARGET_NAME_IF_EXISTS:Qt6::Core>
            $<TARGET_NAME_IF_EXISTS:Qt6::Gui>
            $<TARGET_NAME_IF_EXISTS:Qt6::Widgets>
            $<TARGET_NAME_IF_EXISTS:Qt6::OpenGLWidgets>
            ${OPENGL_LIBRARIES}
            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIO>
            $<TARGET_NAME_IF_EXISTS:OpenColorIO::OpenColorIOHeaders>
            ${OIIO_IV_EXTRA_IV_LIBRARIES}
        )
    if (iv_enabled AND FORCE_OPENGL_1)
        target_compile_definitions(iv PRIVATE FORCE_OPENGL_1)
    endif()
    if (Qt5_FOUND)
        target_compile_definitions(iv PRIVATE OIIO_QT_MAJOR=5)
    endif ()
    if (Qt6_FOUND)
        target_compile_definitions(iv PRIVATE OIIO_QT_MAJOR=6)
    endif ()
    if (APPLE)
        target_compile_definitions(iv PRIVATE GL_SILENCE_DEPRECATION)
    endif ()
else ()
    message (STATUS "\n\n   WARNING: Qt or OpenGL not found -- 'iv' will not be built!\n")
endif ()
