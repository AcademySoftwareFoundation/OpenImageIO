# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (OPENJPEG_FOUND)
    if (TARGET openjp2_static AND (LINKSTATIC OR NOT TARGET openjp2))
        # Use static OpenJPEG library for LINKSTATIC situations, or if
        # no dynamic library is available.
        set (OPENJPEG_TARGET openjp2_static)
    elseif (TARGET openjp2)
        set (OPENJPEG_TARGET openjp2)
    else ()
        set(_jpeg2000_includes ${OPENJPEG_INCLUDES})
        set(_jpeg2000_lib_dirs ${OPENJPEG_LIBRARY_DIRS})
        set(_jpeg2000_libs ${OPENJPEG_LIBRARIES})
    endif ()
    set(_jpeg2000_defs "USE_OPENJPEG")

    if (openjph_FOUND)
        list(APPEND _jpeg2000_defs "USE_OPENJPH")
    endif()

    add_oiio_plugin(jpeg2000input.cpp jpeg2000output.cpp
        INCLUDE_DIRS ${_jpeg2000_includes}
        LINK_DIRECTORIES ${_jpeg2000_lib_dirs}
        LINK_LIBRARIES ${_jpeg2000_libs}
                       ${OPENJPEG_TARGET}
                       $<TARGET_NAME_IF_EXISTS:openjph>
        DEFINITIONS ${_jpeg2000_defs}
    )
else()
    message (WARNING "Jpeg-2000 plugin will not be built")
endif()
