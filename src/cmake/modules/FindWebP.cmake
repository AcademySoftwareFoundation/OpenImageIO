# Module to find Webp
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# This module will first look into the directories defined by the variables:
#   - Webp_ROOT, WEBP_INCLUDE_PATH, WEBP_LIBRARY_PATH
#
# This module defines the following variables:
#
# WebP_FOUND            True if Webp was found.
# WEBP_INCLUDES         Where to find Webp headers
# WEBP_LIBRARIES        List of libraries to link against when using Webp
#
# This doesn't work, because the webp headers don't seem to include any
# definitions giving the version:
# WEBP_VERSION          Version of Webp (e.g., 3.6.2)

# Try the config for newer WebP versions.
find_package(WebP CONFIG)

if (NOT TARGET WebP::webp)
# If not found, roll our own.
# Note: When WebP 1.1 (released late 2019) is our minimum, we can use their
# exported configs and remove our FindWebP.cmake module.

include (FindPackageHandleStandardArgs)

find_path (WEBP_INCLUDE_DIR webp/encode.h
           HINTS
               ${WEBP_INCLUDE_PATH}
               ENV WEBP_INCLUDE_PATH
           DOC "The directory where Webp headers reside")

find_library (WEBP_LIBRARY webp
              HINTS
                  ${WEBP_LIBRARY_PATH}
                  ENV WEBP_LIBRARY_PATH)
find_library (WEBPDEMUX_LIBRARY webpdemux
              HINTS
                  ${WEBP_LIBRARY_PATH}
                  ENV WEBP_LIBRARY_PATH)

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (WebP
    REQUIRED_VARS   WEBP_INCLUDE_DIR
                    WEBP_LIBRARY WEBPDEMUX_LIBRARY
    )

if (WebP_FOUND)
    set (WEBP_INCLUDES "${WEBP_INCLUDE_DIR}")
    set (WEBP_LIBRARIES ${WEBP_LIBRARY} ${WEBPDEMUX_LIBRARY})

    if (NOT TARGET WebP::webp)
        add_library(WebP::webp UNKNOWN IMPORTED)
        set_target_properties(WebP::webp PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDES})
        set_property(TARGET WebP::webp APPEND PROPERTY
            IMPORTED_LOCATION ${WEBP_LIBRARY})
    endif ()
    if (NOT TARGET WebP::webpdemux)
        add_library(WebP::webpdemux UNKNOWN IMPORTED)
        set_target_properties(WebP::webpdemux PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDES})
        set_property(TARGET WebP::webpdemux APPEND PROPERTY
            IMPORTED_LOCATION ${WEBPDEMUX_LIBRARY})
    endif ()
endif ()

mark_as_advanced (
    WEBP_INCLUDE_DIR
    WEBP_LIBRARY
    WEBPDEMUX_LIBRARY
    )

endif ()
