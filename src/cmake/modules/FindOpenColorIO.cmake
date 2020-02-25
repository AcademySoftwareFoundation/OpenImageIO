# Module to find OpenColorIO
#
# This module will first look into the directories hinted by the variables:
#   - OpenColorIO_ROOT, OPENCOLORIO_INCLUDE_PATH, OPENCOLORIO_LIBRARY_PATH
#
# This module defines the following variables:
#
# OPENCOLORIO_FOUND       - True if OpenColorIO was found.
# OPENCOLORIO_INCLUDES    - where to find OpenColorIO.h
# OPENCOLORIO_LIBRARIES   - list of libraries to link against when using OpenColorIO

include (FindPackageHandleStandardArgs)
include (FindPackageMessage)

find_path (OPENCOLORIO_INCLUDE_DIR
    OpenColorIO/OpenColorIO.h
    HINTS
        ${OPENCOLORIO_INCLUDE_PATH}
        ENV OPENCOLORIO_INCLUDE_PATH
    PATHS
        /sw/include
        /opt/local/include
    DOC "The directory where OpenColorIO/OpenColorIO.h resides")

find_library (OPENCOLORIO_LIBRARY
    NAMES OCIO OpenColorIO
    HINTS
        ${OPENCOLORIO_LIBRARY_PATH}
        ENV OPENCOLORIO_LIBRARY_PATH
    PATHS
        /usr/lib64
        /usr/local/lib64
        /sw/lib
        /opt/local/lib
    DOC "The OCIO library")

if (EXISTS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h")
    file(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" TMP
         REGEX "^#define OCIO_VERSION[ \t].*$")
    string (REGEX MATCHALL "[0-9]+[.0-9]+" OPENCOLORIO_VERSION ${TMP})
endif ()

find_package_handle_standard_args (OpenColorIO
    REQUIRED_VARS   OPENCOLORIO_INCLUDE_DIR OPENCOLORIO_LIBRARY
    FOUND_VAR       OPENCOLORIO_FOUND
    VERSION_VAR     OPENCOLORIO_VERSION
    )

if (OPENCOLORIO_FOUND)
    set (OPENCOLORIO_INCLUDES ${OPENCOLORIO_INCLUDE_DIR})
    set (OPENCOLORIO_LIBRARIES ${OPENCOLORIO_LIBRARY})
endif ()

if (OpenColorIO_FOUND AND LINKSTATIC)
    # Is this necessary?
    find_library (TINYXML_LIBRARY NAMES tinyxml)
    if (TINYXML_LIBRARY)
        set (OPENCOLORIO_LIBRARIES "${OPENCOLORIO_LIBRARIES};${TINYXML_LIBRARY}" CACHE STRING "" FORCE)
    endif ()
    find_library (YAML_LIBRARY NAMES yaml-cpp)
    if (YAML_LIBRARY)
        set (OPENCOLORIO_LIBRARIES "${OPENCOLORIO_LIBRARIES};${YAML_LIBRARY}" CACHE STRING "" FORCE)
    endif ()
    find_library (LCMS2_LIBRARY NAMES lcms2)
    if (LCMS2_LIBRARY)
        set (OPENCOLORIO_LIBRARIES "${OPENCOLORIO_LIBRARIES};${LCMS2_LIBRARY}" CACHE STRING "" FORCE)
    endif ()
endif ()

