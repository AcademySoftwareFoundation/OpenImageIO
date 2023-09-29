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
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

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
  set (FFMPEG_INCLUDES ${FFMPEG_AVCODEC_INCLUDE_DIR})

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
endif ()

if (FFMPEG_INCLUDES)
  set (_libavcodec_version_major_h "${FFMPEG_INCLUDES}/libavcodec/version_major.h")
  if (NOT EXISTS "${_libavcodec_version_major_h}")
    set (_libavcodec_version_major_h "${FFMPEG_INCLUDES}/libavcodec/version.h")
  endif()
  file(STRINGS "${_libavcodec_version_major_h}" TMP
       REGEX "^#define LIBAVCODEC_VERSION_MAJOR .*$")
  string (REGEX MATCHALL "[0-9]+[.0-9]+" LIBAVCODEC_VERSION_MAJOR "${TMP}")
  file(STRINGS "${FFMPEG_INCLUDES}/libavcodec/version.h" TMP
       REGEX "^#define LIBAVCODEC_VERSION_MINOR .*$")
  string (REGEX MATCHALL "[0-9]+[.]?[0-9]*" LIBAVCODEC_VERSION_MINOR "${TMP}")
  file(STRINGS "${FFMPEG_INCLUDES}/libavcodec/version.h" TMP
       REGEX "^#define LIBAVCODEC_VERSION_MICRO .*$")
  string (REGEX MATCHALL "[0-9]+[.0-9]+" LIBAVCODEC_VERSION_MICRO "${TMP}")
  set (LIBAVCODEC_VERSION "${LIBAVCODEC_VERSION_MAJOR}.${LIBAVCODEC_VERSION_MINOR}.${LIBAVCODEC_VERSION_MICRO}")
  if (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 60.3.100)
      set (FFMPEG_VERSION 6.0)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 59.37.100)
      set (FFMPEG_VERSION 5.1)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 59.18.100)
      set (FFMPEG_VERSION 5.0)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 58.134.100)
      set (FFMPEG_VERSION 4.4)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 58.91.100)
      set (FFMPEG_VERSION 4.3)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 58.54.100)
      set (FFMPEG_VERSION 4.2)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 58.35.100)
      set (FFMPEG_VERSION 4.1)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 58.18.100)
      set (FFMPEG_VERSION 4.0)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 57.107.100)
      set (FFMPEG_VERSION 3.4)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 57.89.100)
      set (FFMPEG_VERSION 3.3)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 57.64.100)
      set (FFMPEG_VERSION 3.2)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 57.48.100)
      set (FFMPEG_VERSION 3.1)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 57.24.100)
      set (FFMPEG_VERSION 3.0)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 56.60.100)
      set (FFMPEG_VERSION 2.8)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 56.41.100)
      set (FFMPEG_VERSION 2.7)
  elseif (LIBAVCODEC_VERSION VERSION_GREATER_EQUAL 56.26.100)
      set (FFMPEG_VERSION 2.6)
  else ()
      set (FFMPEG_VERSION 1.0)
  endif ()
  set (FFmpeg_VERSION ${FFMPEG_VERSION})
endif ()

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


mark_as_advanced (
    FFMPEG_INCLUDES FFMPEG_LIBRARIES
    )
