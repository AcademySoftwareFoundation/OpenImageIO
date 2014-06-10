# - Find the native PNG includes and library
#
# This module searches libpng, the library for working with PNG images.
#
# It defines the following variables
#  PNG_INCLUDE_DIRS, where to find png.h, etc.
#  PNG_LIBRARIES, the libraries to link against to use PNG.
#  PNG_DEFINITIONS - You should add_definitons(${PNG_DEFINITIONS}) before compiling code that includes png library files.
#  PNG_FOUND, If false, do not try to use PNG.
#  PNG_VERSION_STRING - the version of the PNG library found (since CMake 2.8.8)
# Also defined, but not for general use are
#  PNG_LIBRARY, where to find the PNG library.
# For backward compatiblity the variable PNG_INCLUDE_DIR is also set. It has the same value as PNG_INCLUDE_DIRS.
#
# Since PNG depends on the ZLib compression library, none of the above will be
# defined unless ZLib can be found.

#=============================================================================
# Copyright 2002-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

include (FindPackageHandleStandardArgs)

if(PNG_FIND_QUIETLY)
  set(_FIND_ZLIB_ARG QUIET)
endif()
find_package(ZLIB ${_FIND_ZLIB_ARG})

if(ZLIB_FOUND)
  find_path(PNG_PNG_INCLUDE_DIR png.h
  /usr/local/include/libpng             # OpenBSD
  )
  
  if (PNG_PNG_INCLUDE_DIR AND EXISTS "${PNG_PNG_INCLUDE_DIR}/png.h")
      file(STRINGS "${PNG_PNG_INCLUDE_DIR}/png.h" png_version_str REGEX "^#define[ \t]+PNG_LIBPNG_VER_STRING[ \t]+\".+\"")
      string(REGEX REPLACE "^#define[ \t]+PNG_LIBPNG_VER_STRING[ \t]+\"([^\"]+)\".*" "\\1" PNG_VERSION_STRING "${png_version_str}")
      unset(png_version_str)
  endif ()
  
  string(REPLACE "." "" _PNG_VERSION_STRING ${PNG_VERSION_STRING})
  string(SUBSTRING ${_PNG_VERSION_STRING} 0 2 _PNG_VERSION_STRING )
  set(PNG_NAMES ${PNG_NAMES} png libpng png${_PNG_VERSION_STRING} libpng${_PNG_VERSION_STRING} png${_PNG_VERSION_STRING}d libpng${_PNG_VERSION_STRING}d)
  unset(_PNG_VERSION_STRING)
  find_library(PNG_LIBRARY NAMES ${PNG_NAMES} )

  if (PNG_LIBRARY AND PNG_PNG_INCLUDE_DIR)
      # png.h includes zlib.h. Sigh.
      set(PNG_INCLUDE_DIRS ${PNG_PNG_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} )
      set(PNG_INCLUDE_DIR ${PNG_INCLUDE_DIRS} ) # for backward compatiblity
      set(PNG_LIBRARIES ${PNG_LIBRARY} ${ZLIB_LIBRARY})

      if (CYGWIN)
        if(BUILD_SHARED_LIBS)
           # No need to define PNG_USE_DLL here, because it's default for Cygwin.
        else()
          set (PNG_DEFINITIONS -DPNG_STATIC)
        endif()
      endif ()
  endif ()
endif()

# handle the QUIETLY and REQUIRED arguments and set PNG_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(PNG
                                  REQUIRED_VARS PNG_LIBRARY PNG_PNG_INCLUDE_DIR
                                  VERSION_VAR PNG_VERSION_STRING)

mark_as_advanced(PNG_PNG_INCLUDE_DIR PNG_LIBRARY )
