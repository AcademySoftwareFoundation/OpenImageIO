# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

# Make the build complete for newer ffmpeg versions (3.1.1+) that have
# marked m_format_context->streams[i]->codec as deprecated.
# FIXME -- at some point, come back and figure out how to fix for real
# before the field disappears entirely.
if (NOT MSVC)
    set_source_files_properties (ffmpeginput.cpp
                             PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
endif()

if (FFmpeg_FOUND)
    add_oiio_plugin (ffmpeginput.cpp
                     INCLUDE_DIRS ${FFMPEG_INCLUDES}
                     LINK_LIBRARIES ${FFMPEG_LIBRARIES}
                                    ${BZIP2_LIBRARIES}
                     DEFINITIONS "-DUSE_FFMPEG")
else()
    message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
endif()
