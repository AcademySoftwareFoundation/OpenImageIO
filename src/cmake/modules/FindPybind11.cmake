# Find Pybind11
#
# Sets the usual variables expected for find_package scripts:
#
# PYBIND11_INCLUDES - header location
# PYBIND11_FOUND - true if pybind11 was found.
# PYBIND11_VERSION - version found

find_path (PYBIND11_INCLUDE_DIR pybind11/pybind11.h
           HINTS "${PROJECT_SOURCE_DIR}/ext/pybind11/include"
           )

if (PYBIND11_INCLUDE_DIR)
    set (PYBIND11_INCLUDES ${PYBIND11_INCLUDE_DIR})
    set (PYBIND11_COMMON_FILE "${PYBIND11_INCLUDE_DIR}/pybind11/detail/common.h")
    IF (NOT EXISTS ${PYBIND11_COMMON_FILE})
        set (PYBIND11_COMMON_FILE "${PYBIND11_INCLUDE_DIR}/pybind11/common.h")
    endif ()
    file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_MAJOR ${TMP})
    file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_MINOR ${TMP})
    file(STRINGS "${PYBIND11_COMMON_FILE}" TMP REGEX "^#define PYBIND11_VERSION_PATCH .*$")
    string (REGEX MATCHALL "[0-9]+$" PYBIND11_VERSION_PATCH ${TMP})
    set (PYBIND11_VERSION "${PYBIND11_VERSION_MAJOR}.${PYBIND11_VERSION_MINOR}.${PYBIND11_VERSION_PATCH}")
endif ()


# Support the REQUIRED and QUIET arguments, and set PYBIND11_FOUND if found.
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (Pybind11
                                   VERSION_VAR   PYBIND11_VERSION
                                   REQUIRED_VARS PYBIND11_INCLUDE_DIR)
mark_as_advanced (PYBIND11_INCLUDE_DIR)
