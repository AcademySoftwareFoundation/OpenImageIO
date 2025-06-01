# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (OPENJPEG_FOUND)
    set(_jpeg2000_includes ${OPENJPEG_INCLUDES})
    set(_jpeg2000_lib_dirs ${OPENJPEG_LIBRARY_DIRS})
    set(_jpeg2000_libs ${OPENJPEG_LIBRARIES})
    set(_jpeg2000_defs "USE_OPENJPEG")

    if (OPENJPH_FOUND)
        list(APPEND _jpeg2000_includes ${OPENJPH_INCLUDES})
        list(APPEND _jpeg2000_lib_dirs ${OPENJPH_LIBRARY_DIRS})
        list(APPEND _jpeg2000_libs ${OPENJPH_LIBRARIES})
        list(APPEND _jpeg2000_defs "USE_OPENJPH")
    endif()

    add_oiio_plugin(jpeg2000input.cpp jpeg2000output.cpp
        INCLUDE_DIRS ${_jpeg2000_includes}
        LINK_DIRECTORIES ${_jpeg2000_lib_dirs}
        LINK_LIBRARIES ${_jpeg2000_libs}
        DEFINITIONS ${_jpeg2000_defs}
    )
else()
    message (WARNING "Jpeg-2000 plugin will not be built")
endif()
