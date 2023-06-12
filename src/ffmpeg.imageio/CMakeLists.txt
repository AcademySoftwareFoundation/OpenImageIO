# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio

if (FFmpeg_FOUND)
    add_oiio_plugin (ffmpeginput.cpp
                     INCLUDE_DIRS ${FFMPEG_INCLUDES}
                     LINK_LIBRARIES ${FFMPEG_LIBRARIES}
                                    ${BZIP2_LIBRARIES}
                     DEFINITIONS "-DUSE_FFMPEG"
                                 "-DOIIO_FFMPEG_VERSION=\"${FFMPEG_VERSION}\"")
else()
    message (STATUS "FFmpeg not found: ffmpeg plugin will not be built")
endif()
