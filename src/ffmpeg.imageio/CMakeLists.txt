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

    # Newer ffmpeg versions (3.1.1+) marked some fields and functions (e.g.
    # av_init_packet, m_format_context->streams[i]->codec) as deprecated.
    # Suppress those warnings here so they apply in both the embedded and
    # standalone (EMBEDPLUGINS=0) build paths.
    # FIXME -- at some point, come back and figure out how to fix for real
    # before the fields disappear entirely.
    add_oiio_plugin (ffmpeginput.cpp
                     INCLUDE_DIRS ${FFMPEG_INCLUDES}
                     LINK_LIBRARIES ${FFMPEG_LIBRARIES}
                                    $<TARGET_NAME_IF_EXISTS:BZip2::BZip2>
                     COMPILE_OPTIONS $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wno-deprecated-declarations>
                     DEFINITIONS "USE_FFMPEG"
                                 "-DOIIO_FFMPEG_VERSION=\"${FFMPEG_VERSION}\"")
else()
    message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
endif()
