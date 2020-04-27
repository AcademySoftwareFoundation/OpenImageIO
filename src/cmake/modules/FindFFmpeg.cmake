# - Try to find ffmpeg libraries (libavcodec, libavformat and libavutil)
# Once done this will define
#
#  FFmpeg_FOUND - system has ffmpeg or libav
#  FFMPEG_INCLUDE_DIR - the ffmpeg include directory
#  FFMPEG_LIBRARIES - Link these to use ffmpeg
#  FFMPEG_LIBAVCODEC
#  FFMPEG_LIBAVFORMAT
#  FFMPEG_LIBAVUTIL
#
# Original:
#   Copyright (c) 2008 Andreas Schneider <mail@cynapses.org>
#   Modified for other libraries by Lasse Kärkkäinen <tronic>
#   Modified for Hedgewars by Stepik777
#   Redistribution and use is allowed according to the terms of the New
#   BSD license.
#
# Modifications:
# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

if (FFMPEG_INCLUDES AND FFMPEG_LIBRARIES)
    set (FFmpeg_FOUND TRUE)
else ()

  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(_FFMPEG_AVCODEC QUIET libavcodec)
    pkg_check_modules(_FFMPEG_AVFORMAT QUIET libavformat)
    pkg_check_modules(_FFMPEG_AVUTIL QUIET libavutil)
    pkg_check_modules(_FFMPEG_SWSCALE QUIET libswscale)
  endif (PKG_CONFIG_FOUND)

  find_path(FFMPEG_AVCODEC_INCLUDE_DIR
    NAMES libavcodec/version.h
    HINTS ${_FFMPEG_AVCODEC_INCLUDE_DIRS}
    PATH_SUFFIXES ffmpeg libav
  )

  find_library(FFMPEG_LIBAVCODEC
    NAMES avcodec
    HINTS ${_FFMPEG_AVCODEC_LIBRARY_DIRS} )

  find_library(FFMPEG_LIBAVFORMAT
    NAMES avformat
    HINTS ${_FFMPEG_AVFORMAT_LIBRARY_DIRS} )

  find_library(FFMPEG_LIBAVUTIL
    NAMES avutil
    HINTS ${_FFMPEG_AVUTIL_LIBRARY_DIRS} )

  find_library(FFMPEG_LIBSWSCALE
    NAMES swscale
    HINTS ${_FFMPEG_SWSCALE_LIBRARY_DIRS} )

  include (FindPackageHandleStandardArgs)
  find_package_handle_standard_args (FFmpeg
    REQUIRED_VARS   FFMPEG_LIBAVCODEC
                    FFMPEG_LIBAVFORMAT
                    FFMPEG_AVCODEC_INCLUDE_DIR
    )

  if (FFmpeg_FOUND)
    set(FFMPEG_INCLUDE_DIR ${FFMPEG_AVCODEC_INCLUDE_DIR})
    set(FFMPEG_INCLUDES ${FFMPEG_AVCODEC_INCLUDE_DIR})
    set(FFMPEG_LIBRARIES
      ${FFMPEG_LIBAVCODEC}
      ${FFMPEG_LIBAVFORMAT}
      ${FFMPEG_LIBAVUTIL}
      ${FFMPEG_LIBSWSCALE}
    )
  endif ()

endif ()

mark_as_advanced (
    FFMPEG_INCLUDES FFMPEG_LIBRARIES
    )
