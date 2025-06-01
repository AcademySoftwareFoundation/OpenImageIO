# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Module to find OPENJPH.
#
# This module will first look into the directories defined by the variables:
#   - OPENJPH_ROOT
#
# This module defines the following variables:
#
# OPENJPH_INCLUDES    - where to find ojph_arg.h
# OPENJPH_LIBRARIES   - list of libraries to link against when using OPENJPH.
# OPENJPH_FOUND       - True if OPENJPH was found.
# OPENJPH_VERSION     - Set to the OPENJPH version found
include (FindPackageHandleStandardArgs)
include (FindPackageMessage)
include (SelectLibraryConfigurations)

if(DEFINED OPENJPH_ROOT)
    set(_openjph_pkgconfig_path "${OPENJPH_ROOT}/lib/pkgconfig")
    if(EXISTS "${_openjph_pkgconfig_path}")
        set(ENV{PKG_CONFIG_PATH} "${_openjph_pkgconfig_path}:$ENV{PKG_CONFIG_PATH}")
    endif()
endif()


find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(OPENJPH_PC QUIET openjph)
endif()

if(OPENJPH_PC_FOUND)
    set(OPENJPH_FOUND TRUE)
    set(OPENJPH_VERSION ${OPENJPH_PC_VERSION})
    set(OPENJPH_INCLUDES ${OPENJPH_PC_INCLUDE_DIRS})
    set(OPENJPH_LIBRARY_DIRS ${OPENJPH_PC_LIBDIR})
    set(OPENJPH_LIBRARIES ${OPENJPH_PC_LIBRARIES})

    if(NOT OPENJPH_FIND_QUIETLY)
        FIND_PACKAGE_MESSAGE(OPENJPH
            "Found OPENJPH via pkg-config: v${OPENJPH_VERSION} ${OPENJPH_LIBRARIES}"
            "[${OPENJPH_INCLUDES}][${OPENJPH_LIBRARIES}]"
        )
    endif()
else()
    set(OPENJPH_FOUND FALSE)
    set(OPENJPH_VERSION 0.0.0)
    set(OPENJPH_INCLUDES "")
    set(OPENJPH_LIBRARIES "")
    if(NOT OPENJPH_FIND_QUIETLY)
        FIND_PACKAGE_MESSAGE(OPENJPH
            "Could not find OPENJPH via pkg-config"
            "[${OPENJPH_INCLUDES}][${OPENJPH_LIBRARIES}]"
        )
    endif()
endif()