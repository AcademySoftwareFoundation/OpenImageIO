###########################################################################
# OpenImageIO   https://www.openimageio.org
# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md
#
# For an up-to-date version of this file, see:
#   https://github.com/OpenImageIO/oiio/blob/master/src/cmake/Modules/FindOpenImageIO.cmake
#
###########################################################################
#
# CMake module to find OpenImageIO
#
# This module will set
#   OpenImageIO_FOUND          True, if found
#   OPENIMAGEIO_INCLUDES       directory where headers are found
#   OPENIMAGEIO_LIBRARIES      libraries for OIIO
#   OPENIMAGEIO_LIBRARY_DIRS   library dirs for OIIO
#   OPENIMAGEIO_VERSION        Version ("major.minor.patch.tweak")
#   OPENIMAGEIO_VERSION_MAJOR  Version major number
#   OPENIMAGEIO_VERSION_MINOR  Version minor number
#   OPENIMAGEIO_VERSION_PATCH  Version minor patch
#   OPENIMAGEIO_VERSION_TWEAK  Version minor tweak
#   OIIOTOOL_BIN               Path to oiiotool executable
#
# Imported targets:
#   OpenImageIO::OpenImageIO   The libOpenImageIO library.
#   OpenImageIO::oiiotool      The oiiotool executable.
#
# Special inputs:
#   OpenImageIO_ROOT - if using CMake >= 3.12, will automatically search
#                          this area for OIIO components.
#   OPENIMAGEIO_ROOT_DIR - custom "prefix" location of OIIO installation
#                          (expecting bin, lib, include subdirectories)
#                          This is deprecated, but will work for a while.
#   OpenImageIO_FIND_QUIETLY - if set, print minimal console output
#   OIIO_LIBNAME_SUFFIX - if set, optional nonstandard library suffix
#
###########################################################################
#
# NOTE: This file is deprecated.
#
# In OIIO 2.1+, we generate OpenImageIOConfig.cmake files that are now the
# preferred way for downstream projecs to find an installed OIIO. There
# should be no need to copy this FindOpenImageIO.cmake file into downstream
# projects, *unless* they need to work with a range of OIIO vesions that
# may include <2.1, which would lack the generated config files.
#
###########################################################################


# If 'OPENIMAGE_HOME' not set, use the env variable of that name if available
if (NOT OPENIMAGEIO_ROOT_DIR AND NOT $ENV{OPENIMAGEIO_ROOT_DIR} STREQUAL "")
    set (OPENIMAGEIO_ROOT_DIR $ENV{OPENIMAGEIO_ROOT_DIR})
endif ()


find_library ( OPENIMAGEIO_LIBRARY
               NAMES OpenImageIO${OIIO_LIBNAME_SUFFIX}
               HINTS ${OPENIMAGEIO_ROOT_DIR}
               PATH_SUFFIXES lib64 lib )
find_path ( OPENIMAGEIO_INCLUDE_DIR
            NAMES OpenImageIO/imageio.h
            HINTS ${OPENIMAGEIO_ROOT_DIR} )
find_program ( OIIOTOOL_BIN
               NAMES oiiotool
               HINTS ${OPENIMAGEIO_ROOT_DIR} )

# Try to figure out version number
set (OIIO_VERSION_HEADER "${OPENIMAGEIO_INCLUDE_DIR}/OpenImageIO/oiioversion.h")
if (EXISTS "${OIIO_VERSION_HEADER}")
    file (STRINGS "${OIIO_VERSION_HEADER}" TMP REGEX "^#define OIIO_VERSION_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" OPENIMAGEIO_VERSION_MAJOR ${TMP})
    file (STRINGS "${OIIO_VERSION_HEADER}" TMP REGEX "^#define OIIO_VERSION_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" OPENIMAGEIO_VERSION_MINOR ${TMP})
    file (STRINGS "${OIIO_VERSION_HEADER}" TMP REGEX "^#define OIIO_VERSION_PATCH .*$")
    string (REGEX MATCHALL "[0-9]+" OPENIMAGEIO_VERSION_PATCH ${TMP})
    file (STRINGS "${OIIO_VERSION_HEADER}" TMP REGEX "^#define OIIO_VERSION_TWEAK .*$")
    if (TMP)
        string (REGEX MATCHALL "[0-9]+" OPENIMAGEIO_VERSION_TWEAK ${TMP})
    else ()
        set (OPENIMAGEIO_VERSION_TWEAK 0)
    endif ()
    set (OPENIMAGEIO_VERSION "${OPENIMAGEIO_VERSION_MAJOR}.${OPENIMAGEIO_VERSION_MINOR}.${OPENIMAGEIO_VERSION_PATCH}.${OPENIMAGEIO_VERSION_TWEAK}")
endif ()


include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (OpenImageIO
    FOUND_VAR     OpenImageIO_FOUND
    REQUIRED_VARS OPENIMAGEIO_INCLUDE_DIR OPENIMAGEIO_LIBRARY
                  OPENIMAGEIO_VERSION
    VERSION_VAR   OPENIMAGEIO_VERSION
    )
set (OPENIMAGEIO_FOUND ${OpenImageIO_FOUND})  # Old name

if (OpenImageIO_FOUND)
    set (OPENIMAGEIO_INCLUDES ${OPENIMAGEIO_INCLUDE_DIR})
    set (OPENIMAGEIO_LIBRARIES ${OPENIMAGEIO_LIBRARY})
    get_filename_component (OPENIMAGEIO_LIBRARY_DIRS "${OPENIMAGEIO_LIBRARY}" DIRECTORY)
    if (NOT OpenImageIO_FIND_QUIETLY)
        message ( STATUS "OpenImageIO includes     = ${OPENIMAGEIO_INCLUDE_DIR}" )
        message ( STATUS "OpenImageIO libraries    = ${OPENIMAGEIO_LIBRARIES}" )
        message ( STATUS "OpenImageIO library_dirs = ${OPENIMAGEIO_LIBRARY_DIRS}" )
        message ( STATUS "OpenImageIO oiiotool     = ${OIIOTOOL_BIN}" )
    endif ()

    if (NOT TARGET OpenImageIO::OpenImageIO)
        add_library(OpenImageIO::OpenImageIO UNKNOWN IMPORTED)
        set_target_properties(OpenImageIO::OpenImageIO PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENIMAGEIO_INCLUDES}")

        set_property(TARGET OpenImageIO::OpenImageIO APPEND PROPERTY
            IMPORTED_LOCATION "${OPENIMAGEIO_LIBRARIES}")
    endif ()

    if (NOT TARGET OpenImageIO::oiiotool AND EXISTS "${OIIOTOOL_BIN}")
        add_executable(OpenImageIO::oiiotool IMPORTED)
        set_target_properties(OpenImageIO::oiiotool PROPERTIES
            IMPORTED_LOCATION "${OIIOTOOL_BIN}")
    endif ()
endif ()

mark_as_advanced (
    OPENIMAGEIO_INCLUDE_DIR
    OPENIMAGEIO_LIBRARY
    )
