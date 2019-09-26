# Module to find Field3D
#
# This module will first look into the directories defined by the variables:
#   - Field3D_ROOT, FIELD3D_INCLUDE_PATH, FIELD3D_LIBRARY_PATH
#
# This module defines the following variables:
#
# FIELD3D_FOUND            True if Field3D was found.
# FIELD3D_INCLUDES         Where to find Field3D headers
# FIELD3D_LIBRARIES        List of libraries to link against when using Field3D
# FIELD3D_VERSION          Version of Field3D (e.g., 3.6.2)

include (FindPackageHandleStandardArgs)

find_path (FIELD3D_INCLUDE_DIR Field3D/Field.h
           HINTS
               ${FIELD3D_INCLUDE_PATH}
               ENV FIELD3D_INCLUDE_PATH
           DOC "The directory where Field3D headers reside")

find_library (FIELD3D_LIBRARY Field3D
              HINTS
                  ${FIELD3D_LIBRARY_PATH}
                  ENV FIELD3D_LIBRARY_PATH
              DOC "The Field3D libraries")

find_package_handle_standard_args (FIELD3D
    REQUIRED_VARS   FIELD3D_INCLUDE_DIR FIELD3D_LIBRARY
    )

if (FIELD3D_FOUND)
    set (FIELD3D_INCLUDES ${FIELD3D_INCLUDE_DIR})
    set (FIELD3D_LIBRARIES ${FIELD3D_LIBRARY})
endif ()

mark_as_advanced (
    FIELD3D_INCLUDE_DIR
    FIELD3D_LIBRARY
    )
