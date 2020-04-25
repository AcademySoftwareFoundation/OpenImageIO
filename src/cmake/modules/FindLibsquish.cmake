# Module to find Libsquish
#
# This module will first look into the directories defined by the variables:
#   - Libsquish_ROOT, LIBSQUISH_INCLUDE_PATH, LIBSQUISH_LIBRARY_PATH
#
# This module defines the following variables:
#
# Libsquish_FOUND            True if Libsquish was found.
# LIBSQUISH_INCLUDES         Where to find Libsquish headers
# LIBSQUISH_LIBRARIES        List of libraries to link against when using Libsquish
# LIBSQUISH_VERSION          Version of Libsquish (e.g., 3.6.2)

include (FindPackageHandleStandardArgs)

find_path (LIBSQUISH_INCLUDE_DIR squish.h
           HINTS
               ${LIBSQUISH_INCLUDE_PATH}
               ENV LIBSQUISH_INCLUDE_PATH
           DOC "The directory where Libsquish headers reside")

find_library (LIBSQUISH_LIBRARY squish
              HINTS
                  ${LIBSQUISH_LIBRARY_PATH}
                  ENV LIBSQUISH_LIBRARY_PATH
              DOC "The Libsquish libraries")

find_package_handle_standard_args (Libsquish
    REQUIRED_VARS
        LIBSQUISH_INCLUDE_DIR
        LIBSQUISH_LIBRARY
    )

if (Libsquish_FOUND)
    set (LIBSQUISH_INCLUDES ${LIBSQUISH_INCLUDE_DIR})
    set (LIBSQUISH_LIBRARIES ${LIBSQUISH_LIBRARY})

    if (NOT TARGET Libsquish::Libsquish)
        add_library(Libsquish::Libsquish UNKNOWN IMPORTED)
        set_target_properties(Libsquish::Libsquish PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LIBSQUISH_INCLUDES}")

        set_property(TARGET Libsquish::Libsquish APPEND PROPERTY
            IMPORTED_LOCATION "${LIBSQUISH_LIBRARIES}")
    endif ()
endif ()

mark_as_advanced (
    LIBSQUISH_INCLUDE_DIR
    LIBSQUISH_LIBRARY
    )
