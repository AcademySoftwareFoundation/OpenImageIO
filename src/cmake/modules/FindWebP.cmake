# Module to find Webp
#
# This module will first look into the directories defined by the variables:
#   - Webp_ROOT, WEBP_INCLUDE_PATH, WEBP_LIBRARY_PATH
#
# This module defines the following variables:
#
# Webp_FOUND            True if Webp was found.
# WEBP_INCLUDES         Where to find Webp headers
# WEBP_LIBRARIES        List of libraries to link against when using Webp
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
find_package_handle_standard_args (Webp
    REQUIRED_VARS   WEBP_INCLUDE_DIR
                    WEBP_LIBRARY WEBPDEMUX_LIBRARY
    )

if (Webp_FOUND)
    set (WEBP_INCLUDES "${WEBP_INCLUDE_DIR}")
    set (WEBP_LIBRARIES ${WEBP_LIBRARY} ${WEBPDEMUX_LIBRARY})

    if (NOT TARGET Webp::Webp)
        add_library(Webp::Webp UNKNOWN IMPORTED)
        set_target_properties(Webp::Webp PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDES})
        set_property(TARGET Webp::Webp APPEND PROPERTY
            IMPORTED_LOCATION ${WEBP_LIBRARY})
    endif ()
    if (NOT TARGET Webp::WebpDemux)
        add_library(Webp::WebpDemux UNKNOWN IMPORTED)
        set_target_properties(Webp::WebpDemux PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${WEBP_INCLUDES})
        set_property(TARGET Webp::WebpDemux APPEND PROPERTY
            IMPORTED_LOCATION ${WEBPDEMUX_LIBRARY})
    endif ()
endif ()

mark_as_advanced (
    WEBP_INCLUDE_DIR
    WEBP_LIBRARY
    WEBPDEMUX_LIBRARY
    )
