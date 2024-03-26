# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Module to find OpenColorIO
#
# This module will first look into the directories hinted by the variables:
#   - OpenColorIO_ROOT
#
# This module defines the following targets:
#
#   - OpenColorIO::OpenColorIO
#
# Old style CMake, this module also defines the following variables:
#
# OpenColorIO_FOUND       - True if OpenColorIO was found.
# OPENCOLORIO_INCLUDES    - where to find OpenColorIO.h
# OPENCOLORIO_LIBRARIES   - list of libraries to link against when using OpenColorIO
# OPENCOLORIO_DEFINITIONS - Definitions needed when using OpenColorIO
#
# Hints and overrides:
#
#   - OPENCOLORIO_INTERFACE_LINK_LIBRARIES - override for interface link
#     libraries on the OpenColorIO::OpenColorIO target.
#   - OPENCOLORIO_NO_CONFIG - if ON, this module will be used even if an
#     OCIO >= 2.1 cmake config is found. If OFF (the default), a config file
#     will be preferred if found.
#
# OpenColorIO 2.1 exports proper cmake config files on its own.
# Once OCIO 2.1 is our new minimum, this FindOpenColorIO.cmake will
# eventually be deprecated and disappear.
#

if (NOT OPENCOLORIO_NO_CONFIG)
    find_package(OpenColorIO CONFIG)
endif ()

if (TARGET OpenColorIO::OpenColorIO)
    if (OPENCOLORIO_INTERFACE_LINK_LIBRARIES)
        set_target_properties(OpenColorIO::OpenColorIO PROPERTIES
            INTERFACE_LINK_LIBRARIES "${OPENCOLORIO_INTERFACE_LINK_LIBRARIES}")
    endif ()

else ()
# vvvv the rest is if no OCIO exported config file is found

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

if (EXISTS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h")
    # Search twice, because this symbol changed between OCIO 1.x and 2.x
    file(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" TMP
         REGEX "^#define OCIO_VERSION_STR[ \t].*$")
    if (NOT TMP)
        file(STRINGS "${OPENCOLORIO_INCLUDE_DIR}/OpenColorIO/OpenColorABI.h" TMP
             REGEX "^#define OCIO_VERSION[ \t].*$")
    endif ()
    string (REGEX MATCHALL "([0-9]+)\\.([0-9]+)\\.[0-9]+" OPENCOLORIO_VERSION ${TMP})
    set (OPENCOLORIO_VERSION_MAJOR ${CMAKE_MATCH_1})
    set (OPENCOLORIO_VERSION_MINOR ${CMAKE_MATCH_2})
endif ()

find_library (OPENCOLORIO_LIBRARY
    NAMES
        OpenColorIO
        OpenColorIO_${OPENCOLORIO_VERSION_MAJOR}_${OPENCOLORIO_VERSION_MINOR}
    HINTS
        ${OPENCOLORIO_LIBRARY_PATH}
        ENV OPENCOLORIO_LIBRARY_PATH
    PATHS
        /usr/lib64
        /usr/local/lib64
        /sw/lib
        /opt/local/lib
    DOC "The OCIO library")

find_package_handle_standard_args (OpenColorIO
    REQUIRED_VARS   OPENCOLORIO_INCLUDE_DIR OPENCOLORIO_LIBRARY
    FOUND_VAR       OpenColorIO_FOUND
    VERSION_VAR     OPENCOLORIO_VERSION
    )

if (OpenColorIO_FOUND)
    set (OpenColorIO_VERSION ${OPENCOLORIO_VERSION})
    set (OPENCOLORIO_INCLUDES ${OPENCOLORIO_INCLUDE_DIR})
    set (OPENCOLORIO_LIBRARIES ${OPENCOLORIO_LIBRARY})
    set (OPENCOLORIO_DEFINITIONS "")
    if (NOT TARGET OpenColorIO::OpenColorIO)
        add_library(OpenColorIO::OpenColorIO UNKNOWN IMPORTED)
        set_target_properties(OpenColorIO::OpenColorIO PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENCOLORIO_INCLUDES}")

        set_property(TARGET OpenColorIO::OpenColorIO APPEND PROPERTY
            IMPORTED_LOCATION "${OPENCOLORIO_LIBRARIES}")
        if (OPENCOLORIO_INTERFACE_LINK_LIBRARIES)
            set_target_properties(OpenColorIO::OpenColorIO PROPERTIES
                INTERFACE_LINK_LIBRARIES "${OPENCOLORIO_INTERFACE_LINK_LIBRARIES}")
        endif ()
        if (LINKSTATIC)
            target_compile_definitions(OpenColorIO::OpenColorIO
                INTERFACE "-DOpenColorIO_STATIC")
        endif()
    endif ()
    if (NOT TARGET OpenColorIO::OpenColorIOHeaders)
        add_library(OpenColorIO::OpenColorIOHeaders INTERFACE IMPORTED)
        set_target_properties(OpenColorIO::OpenColorIOHeaders PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENCOLORIO_INCLUDES}")
    endif ()
endif ()

if (OpenColorIO_FOUND AND LINKSTATIC)
    # Is this necessary?
    set (OPENCOLORIO_DEFINITIONS "-DOpenColorIO_STATIC")
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

endif()
