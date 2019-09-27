# Module to find Webp
#
# This module will first look into the directories defined by the variables:
#   - Webp_ROOT, WEBP_INCLUDE_PATH, WEBP_LIBRARY_PATH
#
# This module defines the following variables:
#
# WEBP_FOUND            True if Webp was found.
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
                  ENV WEBP_LIBRARY_PATH
              DOC "The directory where Webp libraries reside")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (WEBP
    REQUIRED_VARS   WEBP_INCLUDE_DIR
                    WEBP_LIBRARY
    )

if (WEBP_FOUND)
    set (WEBP_INCLUDES "${WEBP_INCLUDE_DIR}")
    set (WEBP_LIBRARIES "${WEBP_LIBRARY}")
endif ()

mark_as_advanced (
    WEBP_INCLUDE_DIR
    WEBP_LIBRARY
    )
