# Module to find Webp
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

    if (NOT TARGET WebP::WebP)
        add_library(WebP::WebP UNKNOWN IMPORTED)
        set_target_properties(WebP::WebP PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDES})
        set_property(TARGET WebP::WebP APPEND PROPERTY
            IMPORTED_LOCATION ${WEBP_LIBRARY})
    endif ()
    if (NOT TARGET WebP::WebPDemux)
        add_library(WebP::WebPDemux UNKNOWN IMPORTED)
        set_target_properties(WebP::WebPDemux PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDES})
        set_property(TARGET WebP::WebPDemux APPEND PROPERTY
            IMPORTED_LOCATION ${WEBPDEMUX_LIBRARY})
    endif ()
endif ()

mark_as_advanced (
    WEBP_INCLUDE_DIR
    WEBP_LIBRARY
    WEBPDEMUX_LIBRARY
    )
