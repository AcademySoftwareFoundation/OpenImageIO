# - Find OPENVDB library
# Find the native OPENVDB includes and library
# This module defines
#  OPENVDB_INCLUDE_DIRS, where to find openvdb.h, Set when
#                            OPENVDB_INCLUDE_DIR is found.
#  OPENVDB_LIBRARIES, libraries to link against to use OPENVDB.
#  OpenVDB_ROOT, The base directory to search for OPENVDB.
#                        This can also be an environment variable.
#  OpenVDB_FOUND, If false, do not try to use OPENVDB.
#
# also defined, but not for general use are
#  OPENVDB_LIBRARY, where to find the OPENVDB library.

#=============================================================================
# Modified from one with this copyright notice:
#
# Copyright 2015 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================

SET(_openvdb_SEARCH_DIRS
  ${OpenVDB_ROOT}
  ENV OpenVDB_ROOT
  /usr/local
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/openvdb
  /opt/lib/openvdb
)

FIND_PATH(OPENVDB_INCLUDE_DIR
  NAMES
    openvdb/openvdb.h
  HINTS
    ${_openvdb_SEARCH_DIRS}
  PATH_SUFFIXES
    include
)

if (OPENVDB_INCLUDE_DIR)
    file (STRINGS "${OPENVDB_INCLUDE_DIR}/openvdb/version.h" TMP REGEX "^#define OPENVDB_LIBRARY_MAJOR_VERSION_NUMBER .*$")
    string (REGEX MATCHALL "[0-9]+" OPENVDB_VERSION_MAJOR ${TMP})
    file (STRINGS "${OPENVDB_INCLUDE_DIR}/openvdb/version.h" TMP REGEX "^#define OPENVDB_LIBRARY_MINOR_VERSION_NUMBER .*$")
    string (REGEX MATCHALL "[0-9]+" OPENVDB_VERSION_MINOR ${TMP})
    file (STRINGS "${OPENVDB_INCLUDE_DIR}/openvdb/version.h" TMP REGEX "^#define OPENVDB_LIBRARY_PATCH_VERSION_NUMBER .*$")
    string (REGEX MATCHALL "[0-9]+" OPENVDB_VERSION_PATCH ${TMP})
    set (OPENVDB_VERSION "${OPENVDB_VERSION_MAJOR}.${OPENVDB_VERSION_MINOR}.${OPENVDB_VERSION_PATCH}")
endif ()

LIST(INSERT oiio_vdblib_search 0 lib)
if ($<CONFIG:Debug>)
    list (INSERT oiio_vdblib_search 0 lib/debug)
endif ()

find_library(OPENVDB_LIBRARY
  NAMES
    openvdb
  HINTS
    ${_openvdb_SEARCH_DIRS}
  PATH_SUFFIXES
    ${oiio_vdblib_search}
)

# handle the QUIETLY and REQUIRED arguments and set OpenVDB_FOUND to TRUE if
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenVDB
    REQUIRED_VARS OPENVDB_LIBRARY OPENVDB_INCLUDE_DIR
    VERSION_VAR  OPENVDB_VERSION
    )

if (OpenVDB_FOUND)
    set(OPENVDB_LIBRARIES ${OPENVDB_LIBRARY})
    set(OPENVDB_INCLUDES ${OPENVDB_INCLUDE_DIR})

    if (NOT TARGET OpenVDB::OpenVDB)
        add_library(OpenVDB::OpenVDB UNKNOWN IMPORTED)
        set_target_properties(OpenVDB::OpenVDB PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${OPENVDB_INCLUDES}")
        set_property(TARGET OpenVDB::OpenVDB APPEND PROPERTY
            IMPORTED_LOCATION "${OPENVDB_LIBRARIES}")
    endif ()
endif ()

MARK_AS_ADVANCED(
  OPENVDB_INCLUDE_DIR
  OPENVDB_LIBRARY
)

UNSET(_openvdb_SEARCH_DIRS)
