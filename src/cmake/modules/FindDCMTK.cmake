# FIXME: CMake >= 3.5 has its own FindDCMTK.cmake, so when our minimum
# cmake is at least that, we can remove this file.

# Module to find DCMTK
#
# This module will first look into the directories defined by the variables:
#   - DCMTK_PATH, DCMTK_INCLUDE_PATH, DCMTK_LIBRARY_PATH
#
# This module defines the following variables:
#
# DCMTK_FOUND       - True if DCMTK was found.
# DCMTK_INCLUDES -    where to find DCMTK headers
# DCMTK_LIBRARIES   - list of libraries to link against when using DCMTK

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


if (DCMTK_INCLUDE_DIR AND DCMTK_LIBRARIES)
    set(DCMTK_FOUND TRUE)
    set(DCMTK_INCLUDES "${DCMTK_INCLUDE_DIR}")
    if (NOT DCMTK_FIND_QUIETLY)
        message(STATUS "Found DCMTK library ${DCMTK_LIBRARIES}")
        message(STATUS "Found DCMTK includes ${DCMTK_INCLUDES}")
    endif ()
else()
    set(DCMTK_FOUND FALSE)
    message(STATUS "DCMTK not found. Specify DCMTK_PATH to locate it")
endif()

