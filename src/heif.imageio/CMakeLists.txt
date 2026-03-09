# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (Libheif_FOUND)
    # Some extra care is needed if the libheif we found was static
    set (_static_suffixes .lib .a)
    set (_static_libraries_found 0)
    foreach (_libeheif_library IN LISTS LIBHEIF_LIBRARIES)
        get_filename_component (_ext ${_libeheif_library} LAST_EXT)
        list (FIND _static_suffixes "${_ext}" _index)
        if (${_index} GREATER -1)
            MATH (EXPR _static_libraries_found "${_static_libraries_found}+1")
        endif()
    endforeach()
    if (${_static_libraries_found} GREATER 0)
        message (STATUS "${ColorYellow}")
        message (STATUS "You are linking OpenImageIO against a static version of libheif, which is LGPL")
        message (STATUS "licensed. If you intend to redistribute this build of OpenImageIO, we recommend")
        message (STATUS "that you review the libheif license terms, or you may wish to switch to using a")
        message (STATUS "dynamically-linked libheif.")
        message ("${ColorReset}")
    endif()

    add_oiio_plugin (heifinput.cpp heifoutput.cpp
                     LINK_LIBRARIES heif
                     DEFINITIONS "USE_HEIF=1")
else ()
    message (WARNING "heif plugin will not be built")
endif ()
