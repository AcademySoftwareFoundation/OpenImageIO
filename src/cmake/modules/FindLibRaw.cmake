# - Find LibRaw
# Find the LibRaw library <http://www.libraw.org>
# This module defines
#  LibRaw_VERSION_STRING, the version string of LibRaw
#  LibRaw_INCLUDE_DIR, where to find libraw.h
#  LibRaw_LIBRARIES, the libraries needed to use LibRaw (non-thread-safe)
#  LibRaw_r_LIBRARIES, the libraries needed to use LibRaw (thread-safe)
#  LibRaw_DEFINITIONS, the definitions needed to use LibRaw (non-thread-safe)
#  LibRaw_r_DEFINITIONS, the definitions needed to use LibRaw (thread-safe)
#
# Copyright (c) 2013, Pino Toscano <pino at kde dot org>
# Copyright (c) 2013, Gilles Caulier <caulier dot gilles at gmail dot com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

find_package(PkgConfig)

if(PKG_CONFIG_FOUND AND NOT LibRaw_ROOT AND NOT $ENV{LibRaw_ROOT})
   PKG_CHECK_MODULES(PC_LIBRAW QUIET libraw)
   SET(LibRaw_DEFINITIONS ${PC_LIBRAW_CFLAGS_OTHER})

   PKG_CHECK_MODULES(PC_LIBRAW_R QUIET libraw_r)
   SET(LibRaw_r_DEFINITIONS ${PC_LIBRAW_R_CFLAGS_OTHER})
endif()

find_path(LibRaw_INCLUDE_DIR libraw/libraw.h
          HINTS
          ${LIBRAW_INCLUDEDIR_HINT}
          ${PC_LIBRAW_INCLUDEDIR}
          ${PC_LibRaw_INCLUDE_DIRS}
          PATH_SUFFIXES libraw
         )
find_library(LibRaw_LIBRARIES NAMES raw libraw
             HINTS
             ${LIBRAW_LIBDIR_HINT}
             ${PC_LIBRAW_LIBDIR}
             ${PC_LIBRAW_LIBRARY_DIRS}
            )

find_library(LibRaw_r_LIBRARIES NAMES raw_r
             HINTS
             ${LIBRAW_LIBDIR_HINT}
             ${PC_LIBRAW_R_LIBDIR}
             ${PC_LIBRAW_R_LIBRARY_DIRS}
            )

if(WIN32)
   SET( LibRaw_r_LIBRARIES ${LibRaw_LIBRARIES} )
endif()

if(LibRaw_INCLUDE_DIR)
   FILE(READ ${LibRaw_INCLUDE_DIR}/libraw/libraw_version.h _libraw_version_content)
   
   STRING(REGEX MATCH "#define LIBRAW_MAJOR_VERSION[ \t]*([0-9]*)\n" _version_major_match ${_libraw_version_content})
   SET(_libraw_version_major "${CMAKE_MATCH_1}")
   
   STRING(REGEX MATCH "#define LIBRAW_MINOR_VERSION[ \t]*([0-9]*)\n" _version_minor_match ${_libraw_version_content})
   SET(_libraw_version_minor "${CMAKE_MATCH_1}")
   
   STRING(REGEX MATCH "#define LIBRAW_PATCH_VERSION[ \t]*([0-9]*)\n" _version_patch_match ${_libraw_version_content})
   SET(_libraw_version_patch "${CMAKE_MATCH_1}")
   
   if(_version_major_match AND _version_minor_match AND _version_patch_match)
      SET(LibRaw_VERSION_STRING "${_libraw_version_major}.${_libraw_version_minor}.${_libraw_version_patch}")
   else()
      if(NOT LibRaw_FIND_QUIETLY)
         MESSAGE(STATUS "Failed to get version information from ${LibRaw_INCLUDE_DIR}/libraw/libraw_version.h")
      endif()
   endif()
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(LibRaw
                                  REQUIRED_VARS LibRaw_LIBRARIES LibRaw_r_LIBRARIES LibRaw_INCLUDE_DIR
                                  VERSION_VAR LibRaw_VERSION_STRING
                                 )

MARK_AS_ADVANCED(LibRaw_VERSION_STRING
                 LibRaw_INCLUDE_DIR
                 LibRaw_LIBRARIES
                 LibRaw_r_LIBRARIES
                 LibRaw_DEFINITIONS
                 LibRaw_r_DEFINITIONS
                 )

if (LINKSTATIC)
    # Necessary?
    find_package (Jasper)
    find_library (LCMS2_LIBRARIES NAMES lcms2)
    set (LibRaw_r_LIBRARIES ${LibRaw_r_LIBRARIES} ${JASPER_LIBRARIES} ${LCMS2_LIBRARIES})
endif ()
