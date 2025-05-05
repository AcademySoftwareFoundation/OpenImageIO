# Module to find libuhdr
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# This module defines the following variables:
#
# libuhdr_FOUND            True if libuhdr was found.
# LIBUHDR_INCLUDE_DIR      Where to find libuhdr headers
# LIBUHDR_LIBRARY          Library for uhdr
# LIBUHDR_VERSION          Version of the library

include (FindPackageHandleStandardArgs)

find_path(LIBUHDR_INCLUDE_DIR
  NAMES
    ultrahdr_api.h
  PATH_SUFFIXES
    include
)

find_library(LIBUHDR_LIBRARY uhdr
  PATH_SUFFIXES
    lib
)

if (LIBUHDR_INCLUDE_DIR)
    file (STRINGS "${LIBUHDR_INCLUDE_DIR}/ultrahdr_api.h" TMP REGEX "^#define UHDR_LIB_VER_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" LIBUHDR_VERSION_MAJOR ${TMP})
    file (STRINGS "${LIBUHDR_INCLUDE_DIR}/ultrahdr_api.h" TMP REGEX "^#define UHDR_LIB_VER_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" LIBUHDR_VERSION_MINOR ${TMP})
    file (STRINGS "${LIBUHDR_INCLUDE_DIR}/ultrahdr_api.h" TMP REGEX "^#define UHDR_LIB_VER_PATCH .*$")
    string (REGEX MATCHALL "[0-9]+" LIBUHDR_VERSION_PATCH ${TMP})
    set (LIBUHDR_VERSION "${LIBUHDR_VERSION_MAJOR}.${LIBUHDR_VERSION_MINOR}.${LIBUHDR_VERSION_PATCH}")
endif ()

find_package_handle_standard_args (libuhdr
    REQUIRED_VARS   LIBUHDR_INCLUDE_DIR
                    LIBUHDR_LIBRARY
    VERSION_VAR     LIBUHDR_VERSION
    )

if (LIBUHDR_FOUND)
    set(LIBUHDR_LIBRARIES ${LIBUHDR_LIBRARY})
    set(LIBUHDR_INCLUDES ${LIBUHDR_INCLUDE_DIR})
    if (NOT TARGET libuhdr::libuhdr)
        add_library(libuhdr::libuhdr UNKNOWN IMPORTED)
        set_target_properties(libuhdr::libuhdr PROPERTIES
                              INTERFACE_INCLUDE_DIRECTORIES "${LIBUHDR_INCLUDES}")
        set_property(TARGET libuhdr::libuhdr APPEND PROPERTY
                     IMPORTED_LOCATION "${LIBUHDR_LIBRARIES}")
    endif ()
else ()
    unset (LIBUHDR_INCLUDE_DIR)
    unset (LIBUHDR_LIBRARY)
    unset (LIBUHDR_VERSION)
endif()
