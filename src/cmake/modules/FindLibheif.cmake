# Module to find LIBHEIF
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# This module will first look into the directories defined by the variables:
#   - Libheif_ROOT, LIBHEIF_INCLUDE_PATH, LIBHEIF_LIBRARY_PATH
#
# This module defines the following variables:
#
# Libheif_FOUND            True if LIBHEIF was found.
# LIBHEIF_INCLUDES         Where to find LIBHEIF headers
# LIBHEIF_LIBRARIES        List of libraries to link against when using LIBHEIF
# LIBHEIF_VERSION          Version of LIBHEIF (e.g., 3.6.2)

include (FindPackageHandleStandardArgs)

find_path (LIBHEIF_INCLUDE_DIR
    libheif/heif_version-64.h
    HINTS
        ${LIBHEIF_INCLUDE_PATH}
        ENV LIBHEIF_INCLUDE_PATH
    DOC "The directory where libheif headers reside")

find_library (LIBHEIF_LIBRARY heif
              HINTS
                  ${LIBHEIF_LIBRARY_PATH}
                  ENV LIBHEIF_LIBRARY_PATH
              DOC "The found libheif library")

if (LIBHEIF_INCLUDE_DIR)
    file(STRINGS "${LIBHEIF_INCLUDE_DIR}/libheif/heif_version-64.h" TMP REGEX "^#define LIBHEIF_VERSION[ \t].*$")
    string(REGEX MATCHALL "[0-9.]+" LIBHEIF_VERSION ${TMP})
endif ()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (Libheif
    REQUIRED_VARS   LIBHEIF_INCLUDE_DIR
                    LIBHEIF_LIBRARY
    )

if (Libheif_FOUND)
    set(LIBHEIF_INCLUDES "${LIBHEIF_INCLUDE_DIR}")
    set(LIBHEIF_LIBRARIES "${LIBHEIF_LIBRARY}")

    if (NOT TARGET heif)
        add_library(heif UNKNOWN IMPORTED)
        set_target_properties(heif PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LIBHEIF_INCLUDES}")
        set_property(TARGET heif APPEND PROPERTY
            IMPORTED_LOCATION "${LIBHEIF_LIBRARIES}")
    endif ()
endif()

mark_as_advanced (
    LIBHEIF_INCLUDES
    LIBHEIF_LIBRARIES
    LIBHEIF_VERSION
    )
