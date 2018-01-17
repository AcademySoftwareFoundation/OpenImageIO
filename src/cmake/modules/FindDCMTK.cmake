# Module to find DCMTK
#
# This module will first look into the directories defined by the variables:
#   - DCMTK_PATH, DCMTK_INCLUDE_PATH, DCMTK_LIBRARY_PATH
#
# This module defines the following variables:
#
# DCMTK_FOUND            True if DCMTK was found.
# DCMTK_INCLUDES         Where to find DCMTK headers
# DCMTK_LIBRARIES        List of libraries to link against when using DCMTK
# DCMTK_VERSION          Version of DCMTK (e.g., 3.6.2)
# DCMTK_VERSION_NUMBER   Int version of DCMTK (e.g., 362 for 3.6.2)

include (FindPackageHandleStandardArgs)
include (FindPackageMessage)

if (NOT DCMTK_FIND_QUIETLY)
    if (DCMTK_PATH)
        message(STATUS "DCMTK path explicitly specified: ${DCMTK_PATH}")
    endif()
    if (DCMTK_INCLUDE_PATH)
        message(STATUS "DCMTK INCLUDE_PATH explicitly specified: ${DCMTK_INCLUDE_PATH}")
    endif()
    if (DCMTK_LIBRARY_PATH)
        message(STATUS "DCMTK LIBRARY_PATH explicitly specified: ${DCMTK_LIBRARY_PATH}")
    endif()
endif ()

find_path (DCMTK_INCLUDE_DIR
    dcmtk/dcmdata/dcuid.h
    PATHS
    ${DCMTK_INCLUDE_PATH}
    ${DCMTK_PATH}/include/
    /usr/include
    /usr/local/include
    /sw/include
    /opt/local/include
    DOC "The directory where DCMTK headers reside")

foreach (COMPONENT dcmimage dcmimgle dcmdata oflog ofstd iconv)
    find_library (DCMTK_${COMPONENT}_LIB ${COMPONENT}
                  PATHS ${DCMTK_PATH}/lib/
                        /usr/local/lib
                        /usr/lib64
                        /usr/lib
                        /usr/local/lib64
                        /sw/lib
                        /opt/local/lib
                  )
    if (DCMTK_${COMPONENT}_LIB)
        set (DCMTK_LIBRARIES ${DCMTK_LIBRARIES} ${DCMTK_${COMPONENT}_LIB})
    endif ()
endforeach()

message (STATUS "DCMTK_INCLUDE_DIR = ${DCMTK_INCLUDE_DIR}")
if (DCMTK_INCLUDE_DIR AND EXISTS "${DCMTK_INCLUDE_DIR}/dcmtk/config/osconfig.h")
    file(STRINGS "${DCMTK_INCLUDE_DIR}/dcmtk/config/osconfig.h" TMP REGEX "^#define PACKAGE_VERSION[ \t].*$")
    string(REGEX MATCHALL "[0-9.]+" DCMTK_VERSION ${TMP})
    file(STRINGS "${DCMTK_INCLUDE_DIR}/dcmtk/config/osconfig.h" TMP REGEX "^#define PACKAGE_VERSION_NUMBER[ \t].*$")
    string(REGEX MATCHALL "[0-9.]+" DCMTK_VERSION_NUMBER ${TMP})
endif ()

if (DCMTK_INCLUDE_DIR AND DCMTK_LIBRARIES)
    set(DCMTK_FOUND TRUE)
    set(DCMTK_INCLUDES "${DCMTK_INCLUDE_DIR}")
    if (NOT DCMTK_FIND_QUIETLY)
        message(STATUS "Found DCMTK library ${DCMTK_LIBRARIES}")
        message(STATUS "Found DCMTK includes ${DCMTK_INCLUDES}")
        message(STATUS "Found DCMTK short version number ${DCMTK_VERSION_NUMBER}")
    endif ()
else()
    set(DCMTK_FOUND FALSE)
    message(STATUS "DCMTK not found. Specify DCMTK_PATH to locate it")
endif()

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (DCMTK
    REQUIRED_VARS   DCMTK_INCLUDE_DIR DCMTK_LIBRARIES
    VERSION_VAR     DCMTK_VERSION
    )

mark_as_advanced (
    DCMTK_INCLUDE_DIR
    DCMTK_LIBRARIES
    DCMTK_VERSION
    DCMTK_VERSION_NUMBER
    )
