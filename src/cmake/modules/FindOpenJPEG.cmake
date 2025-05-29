# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO

# Module to find OpenJpeg.
#
# This module will first look into the directories defined by the variables:
#   - OpenJPEG_ROOT
#
# This module defines the following variables:
#
# OPENJPEG_INCLUDES    - where to find openjpeg.h
# OPENJPEG_LIBRARIES   - list of libraries to link against when using OpenJpeg.
# OPENJPEG_FOUND       - True if OpenJpeg was found.
# OPENJPEG_VERSION     - Set to the OpenJPEG version found
include (FindPackageHandleStandardArgs)
include (FindPackageMessage)
include (SelectLibraryConfigurations)



if(DEFINED OPENJPEG_ROOT)
    set(_openjpeg_pkgconfig_path "${OPENJPEG_ROOT}/lib/pkgconfig")
    if(EXISTS "${_openjpeg_pkgconfig_path}")
        set(ENV{PKG_CONFIG_PATH} "${_openjpeg_pkgconfig_path}:$ENV{PKG_CONFIG_PATH}")
    endif()
endif()


find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(OPENJPEG_PC QUIET openjpeg)
endif()

if(OPENJPEG_PC_FOUND)
    set(OPENJPEG_FOUND TRUE)
    set(OPENJPEG_VERSION ${OPENJPEG_PC_VERSION})
    set(OPENJPEG_INCLUDES ${OPENJPEG_PC_INCLUDE_DIRS})
    set(OPENJPEG_LIBRARIES ${OPENJPEG_PC_LIBRARIES})
    if(NOT OPENJPEG_FIND_QUIETLY)
        FIND_PACKAGE_MESSAGE(OPENJPEG
            "Found OPENJPEG via pkg-config: v${OPENJPEG_VERSION} ${OPENJPEG_LIBRARIES}"
            "[${OPENJPEG_INCLUDES}][${OPENJPEG_LIBRARIES}]"
        )
    endif()
else()
    set(OPENJPEG_FOUND FALSE)
    set(OPENJPEG_VERSION 0.0.0)
    set(OPENJPEG_INCLUDES "")
    set(OPENJPEG_LIBRARIES "")
    if(NOT OPENJPEG_FIND_QUIETLY)
        FIND_PACKAGE_MESSAGE(OPENJPEG
            "Could not find OPENJPEG via pkg-config"
            "[${OPENJPEG_INCLUDES}][${OPENJPEG_LIBRARIES}]"
        )
    endif()
endif()


macro (PREFIX_FIND_INCLUDE_DIR prefix includefile libpath_var)
  string (TOUPPER ${prefix}_INCLUDE_DIR tmp_varname)
  find_path(${tmp_varname} ${includefile}
    PATHS ${${libpath_var}}
    PATH_SUFFIXES openjpeg openjpeg-2.0 openjpeg-2.1 openjpeg-2.2
                  openjpeg-2.3 openjpeg-2.4 openjpeg-2.5
  )
  if (${tmp_varname})
    mark_as_advanced (${tmp_varname})
  endif ()
  unset (tmp_varname)
endmacro ()


macro (PREFIX_FIND_LIB prefix libname libpath_var liblist_var cachelist_var)
  string (TOUPPER ${prefix}_${libname} tmp_prefix)
  find_library(${tmp_prefix}_LIBRARY_RELEASE
    NAMES ${libname}
    PATHS ${${libpath_var}}
  )
  find_library(${tmp_prefix}_LIBRARY_DEBUG
    NAMES ${libname}d ${libname}_d ${libname}debug ${libname}_debug
    PATHS ${${libpath_var}}
  )
  # Properly define ${tmp_prefix}_LIBRARY (cached) and ${tmp_prefix}_LIBRARIES
  select_library_configurations (${tmp_prefix})
  list (APPEND ${liblist_var} ${tmp_prefix}_LIBRARIES)

  # Add to the list of variables which should be reset
  list (APPEND ${cachelist_var}
    ${tmp_prefix}_LIBRARY
    ${tmp_prefix}_LIBRARY_RELEASE
    ${tmp_prefix}_LIBRARY_DEBUG)
  mark_as_advanced (
    ${tmp_prefix}_LIBRARY
    ${tmp_prefix}_LIBRARY_RELEASE
    ${tmp_prefix}_LIBRARY_DEBUG)
  unset (tmp_prefix)
endmacro ()

# Generic search paths
set (OpenJpeg_include_paths
     /usr/include
     /opt/local/include
    )

set (OpenJpeg_library_paths
  /usr/lib
  /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
  /usr/local/lib
  /sw/lib
  /opt/local/lib)



# Locate the header files
PREFIX_FIND_INCLUDE_DIR (OpenJpeg openjpeg.h OpenJpeg_include_paths)

# If the headers were found, add its parent to the list of lib directories
if (OPENJPEG_INCLUDE_DIR)
  get_filename_component (tmp_extra_dir "${OPENJPEG_INCLUDE_DIR}/../" ABSOLUTE)
  list (APPEND OpenJPEG_library_paths ${tmp_extra_dir})
  unset (tmp_extra_dir)
endif ()

# Search for opj_config.h -- it is only part of OpenJpeg >= 2.0, and will
# contain symbols OPJ_VERSION_MAJOR, OPJ_VERSION_MINOR, and OBJ_VERSION_BUILD
# since OpenJpeg >= 2.1.
set (OPENJPEG_CONFIG_FILE "${OPENJPEG_INCLUDE_DIR}/opj_config.h")
if (EXISTS "${OPENJPEG_CONFIG_FILE}")
    file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_PACKAGE_VERSION .*$")
    if (TMP)
        # 2.0 is the only one with this construct
        set (OPJ_VERSION_MAJOR 2)
        set (OPJ_VERSION_MINOR 0)
        set (OPJ_VERSION_BUILD 0)
    else ()
        # 2.1 and beyond
        file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_VERSION_MAJOR .*$")
        string (REGEX MATCHALL "[0-9]+" OPJ_VERSION_MAJOR ${TMP})
        file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_VERSION_MINOR .*$")
        string (REGEX MATCHALL "[0-9]+" OPJ_VERSION_MINOR ${TMP})
        file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_VERSION_BUILD .*$")
        string (REGEX MATCHALL "[0-9]+" OPJ_VERSION_BUILD ${TMP})
    endif ()
else ()
    # Guess OpenJPEG 1.5 -- older versions didn't have the version readily
    # apparent in the headers.
    set (OPJ_VERSION_MAJOR 1)
    set (OPJ_VERSION_MINOR 5)
    set (OPJ_VERSION_BUILD 0)
endif ()
set (OPENJPEG_VERSION "${OPJ_VERSION_MAJOR}.${OPJ_VERSION_MINOR}.${OPJ_VERSION_BUILD}")


# Locate the OpenJpeg library
set (OpenJpeg_libvars "")
set (OpenJpeg_cachevars "")
if ("${OPENJPEG_VERSION}" VERSION_LESS 2.0)
    PREFIX_FIND_LIB (OpenJpeg openjpeg
      OpenJpeg_library_paths OpenJpeg_libvars OpenJpeg_cachevars)
else ()
    PREFIX_FIND_LIB (OpenJpeg openjp2
      OpenJpeg_library_paths OpenJpeg_libvars OpenJpeg_cachevars)
endif ()

# Use the standard function to handle OPENJPEG_FOUND
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenJPEG
  VERSION_VAR OPENJPEG_VERSION
  REQUIRED_VARS OPENJPEG_INCLUDE_DIR ${OpenJpeg_libvars})

if (OPENJPEG_FOUND)
  set (OPENJPEG_INCLUDES ${OPENJPEG_INCLUDE_DIR})
  set (OPENJPEG_LIBRARIES "")
  foreach (tmplib ${OpenJpeg_libvars})
    list (APPEND OPENJPEG_LIBRARIES ${${tmplib}})
  endforeach ()
  if (NOT OpenJpeg_FIND_QUIETLY)
    FIND_PACKAGE_MESSAGE (OPENJPEG
      "Found OpenJpeg: v${OPENJPEG_VERSION} ${OPENJPEG_LIBRARIES}"
      "[${OPENJPEG_INCLUDE_DIR}][${OPENJPEG_LIBRARIES}]"
      )
  endif ()
endif ()

unset (OpenJpeg_include_paths)
unset (OpenJpeg_library_paths)
unset (OpenJpeg_libvars)
unset (OpenJpeg_cachevars)
