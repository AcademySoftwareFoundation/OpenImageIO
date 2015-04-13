# Module to find OpenColorIO
#
# This module will first look into the directories defined by the variables:
#   - OCIO_PATH, OCIO_INCLUDE_PATH, OCIO_LIBRARY_PATH
#
# This module defines the following variables:
#
# OCIO_FOUND       - True if OpenColorIO was found.
# OCIO_INCLUDES -    where to find OpenColorIO.h
# OCIO_LIBRARIES   - list of libraries to link against when using OpenColorIO

# Other standarnd issue macros
include (FindPackageHandleStandardArgs)
include (FindPackageMessage)

    if (VERBOSE)
        if (OCIO_PATH)
            message(STATUS "OCIO path explicitly specified: ${OCIO_PATH}")
        endif()
        if (OCIO_INCLUDE_PATH)
            message(STATUS "OCIO INCLUDE_PATH explicitly specified: ${OCIO_INCLUDE_PATH}")
        endif()
        if (OCIO_LIBRARY_PATH)
            message(STATUS "OCIO LIBRARY_PATH explicitly specified: ${OCIO_LIBRARY_PATH}")
        endif()
    endif ()
    FIND_PATH( OCIO_INCLUDES OpenColorIO/OpenColorIO.h
        ${OCIO_INCLUDE_PATH}
        ${OCIO_PATH}/include/
        /usr/include
        /usr/local/include
        /sw/include
        /opt/local/include
        DOC "The directory where OpenColorIO/OpenColorIO.h resides")
    FIND_LIBRARY(OCIO_LIBRARIES
        NAMES OCIO OpenColorIO
        PATHS
        ${OCIO_LIBRARY_PATH}
        ${OCIO_PATH}/lib/
        /usr/lib64
        /usr/lib
        /usr/local/lib64
        /usr/local/lib
        /sw/lib
        /opt/local/lib
        DOC "The OCIO library")
    option(OCIO_STATIC OFF "Use STATIC lib mean use specific config for use")
    if(OCIO_STATIC)
        add_definitions(-DOpenColorIO_STATIC)
    endif()
    option(OCIO_TINYXML_STATIC OFF "Use STATIC lib mean use specific config for use")
    if(OCIO_TINYXML_STATIC)
        add_definitions(-DTIXML_USE_STL)
    endif()
    option(OCIO_YAML_STATIC OFF "How yaml was build to use specific export config under windows")
    if(NOT OCIO_YAML_STATIC)
        add_definitions(-DYAML_CPP_DLL)
    endif()
    if(OCIO_INCLUDES AND OCIO_LIBRARIES)
        set(OCIO_FOUND TRUE)
        if (VERBOSE)
            message(STATUS "Found OCIO library ${OCIO_LIBRARIES}")
            message(STATUS "Found OCIO includes ${OCIO_INCLUDES}")
        endif ()
    else()
        set(OCIO_FOUND FALSE)
        message(STATUS "OCIO not found. Specify OCIO_PATH to locate it")
    endif()

