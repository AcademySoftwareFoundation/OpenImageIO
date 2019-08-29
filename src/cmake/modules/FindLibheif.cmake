# Module to find LIBHEIF
#
# This module will first look into the directories defined by the variables:
#   - LIBHEIF_PATH, LIBHEIF_INCLUDE_PATH, LIBHEIF_LIBRARY_PATH
#
# This module defines the following variables:
#
# LIBHEIF_FOUND            True if LIBHEIF was found.
# LIBHEIF_INCLUDES         Where to find LIBHEIF headers
# LIBHEIF_LIBRARIES        List of libraries to link against when using LIBHEIF
# LIBHEIF_VERSION          Version of LIBHEIF (e.g., 3.6.2)

include (FindPackageHandleStandardArgs)
include (FindPackageMessage)

find_path (LIBHEIF_INCLUDE_DIR
    libheif/heif_version.h
    HINTS
    ${LIBHEIF_INCLUDE_PATH}
    ENV LIBHEIF_INCLUDE_PATH
    PATH_SUFFIXES include
    DOC "The directory where libheif headers reside")

message(STATUS "LIBHEIF_PATH ${LIBHEIF_PATH}")
find_library (LIBHEIF_LIBRARY heif
              HINTS
              ${LIBHEIF_LIBRARY_PATH}
              ENV LIBHEIF_LIBRARY_PATH
              PATH_SUFFIXES lib
              DOC "The directory where libheif libraries reside")

message (STATUS "LIBHEIF_INCLUDE_DIR = ${LIBHEIF_INCLUDE_DIR}")
if (LIBHEIF_INCLUDE_DIR)
    file(STRINGS "${LIBHEIF_INCLUDE_DIR}/libheif/heif_version.h" TMP REGEX "^#define LIBHEIF_VERSION[ \t].*$")
    string(REGEX MATCHALL "[0-9.]+" LIBHEIF_VERSION ${TMP})
endif ()

if (LIBHEIF_INCLUDE_DIR AND LIBHEIF_LIBRARY)
    set(LIBHEIF_FOUND TRUE)
    set(LIBHEIF_INCLUDES "${LIBHEIF_INCLUDE_DIR}")
    set(LIBHEIF_LIBRARIES "${LIBHEIF_LIBRARY}")
    if (NOT LIBHEIF_FIND_QUIETLY)
        message(STATUS "Found libheif ${LIBHEIF_VERSION} library ${LIBHEIF_LIBRARIES}")
        message(STATUS "Found libheif includes ${LIBHEIF_INCLUDES}")
    endif ()
else()
    set(LIBHEIF_FOUND FALSE)
    message(STATUS "libheif not found. Specify LIBHEIF_PATH to locate it")
endif()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (LIBHEIF
    REQUIRED_VARS   LIBHEIF_INCLUDES
                    LIBHEIF_LIBRARIES
    VERSION_VAR     LIBHEIF_VERSION
    )

mark_as_advanced (
    LIBHEIF_INCLUDES
    LIBHEIF_LIBRARIES
    LIBHEIF_VERSION
    )
