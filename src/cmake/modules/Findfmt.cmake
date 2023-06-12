# Find fmt library
#
# Sets the usual variables expected for find_package scripts:
#
# FMT_INCLUDES - header location
# FMT_FOUND - true if fmt was found.
# FMT_VERSION - combined version number (e.g. 60102 for 6.1.2)

find_path (FMT_INCLUDE_DIR fmt/format.h
           HINTS "${PROJECT_SOURCE_DIR}/ext/fmt"
           )

if (FMT_INCLUDE_DIR)
    file(STRINGS "${FMT_INCLUDE_DIR}/fmt/core.h" TMP REGEX "^#define FMT_VERSION .*$")
    string (REGEX MATCHALL "[0-9]+[.0-9]+" FMT_VERSION ${TMP})
endif ()

# Support the REQUIRED and QUIET arguments, and set FMT_FOUND if found.
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (fmt
                                   REQUIRED_VARS FMT_INCLUDE_DIR
                                   VERSION_VAR   FMT_VERSION)

if (FMT_FOUND)
    set (FMT_INCLUDES ${FMT_INCLUDE_DIR})
endif ()

mark_as_advanced (FMT_INCLUDE_DIR)
