# Find Robinmap
#
# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO
#
# Sets the usual variables expected for find_package scripts:
#
# ROBINMAP_INCLUDES - header location
# ROBINMAP_FOUND - true if robin-map was found.

find_path (ROBINMAP_INCLUDE_DIR tsl/robin_map.h
           HINTS "${PROJECT_SOURCE_DIR}/ext/robin-map"
           )

# Support the REQUIRED and QUIET arguments, and set ROBINMAP_FOUND if found.
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (Robinmap
                                   REQUIRED_VARS ROBINMAP_INCLUDE_DIR)

if (ROBINMAP_FOUND)
    set (ROBINMAP_INCLUDES ${ROBINMAP_INCLUDE_DIR})
endif ()

mark_as_advanced (ROBINMAP_INCLUDE_DIR)
