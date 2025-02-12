# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

if (FFmpeg_FOUND)
    if (LINKSTATIC)
        set (_static_suffixes .lib .a)
        set (_static_libraries_found 0)

        foreach (_ffmpeg_library IN LISTS FFMPEG_LIBRARIES)
            get_filename_component (_ext ${_ffmpeg_library} LAST_EXT)
            list (FIND _static_suffixes ${_ext} _index)
            if (${_index} GREATER -1)
                MATH (EXPR _static_libraries_found "${_static_libraries_found}+1")
            endif()
        endforeach()

        if (${_static_libraries_found} GREATER 0)
            message (STATUS "${ColorYellow}")
            message (STATUS "You are linking OpenImageIO against a static version of FFmpeg, which may have")
            message (STATUS "LGPL and possibly GPL licensed components (depending on exactly how your copy")
            message (STATUS "of FFmpeg was built). If you intend to redistribute this build of OpenImageIO,")
            message (STATUS "we recommend that you review the FFmpeg build flags and licensing terms.")        
            message ("${ColorReset}")
        endif()
    endif()

    add_oiio_plugin (ffmpeginput.cpp
                     INCLUDE_DIRS ${FFMPEG_INCLUDES}
                     LINK_LIBRARIES ${FFMPEG_LIBRARIES}
                                    ${BZIP2_LIBRARIES}
                     DEFINITIONS "USE_FFMPEG"
                                 "-DOIIO_FFMPEG_VERSION=\"${FFMPEG_VERSION}\"")
else()
    message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
endif()
