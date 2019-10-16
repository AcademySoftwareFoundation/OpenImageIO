# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (Qt5_FOUND AND OPENGL_FOUND)
    set (CMAKE_AUTOMOC ON)
	if (Qt5_POSITION_INDEPENDENT_CODE)
        set (CMAKE_POSITION_INDEPENDENT_CODE ON)
	endif()
    set (iv_srcs imageviewer.cpp ivimage.cpp ivgl.cpp ivinfowin.cpp
                 ivpref.cpp ivmain.cpp)
    add_executable (iv ${iv_srcs})
    target_include_directories (iv SYSTEM PRIVATE ${OPENGL_INCLUDE_DIR})
    if (FORCE_OPENGL_1)
        target_compile_definitions(iv PRIVATE FORCE_OPENGL_1)
    endif()
    target_link_libraries (iv PRIVATE OpenImageIO
                                   Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL
                                   ${OPENGL_LIBRARIES})
    set_target_properties (iv PROPERTIES FOLDER "Tools")

    # C++ >= 11 doesn't like 'register' keyword, which is in Qt headers
    # target_compile_options (iv PRIVATE -Wno-deprecated-register)

    oiio_install_targets (iv)

else ()
    message (STATUS "\n\n   WARNING: Qt or OpenGL not found -- 'iv' will not be built!\n")
endif ()
