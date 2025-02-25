# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause and Apache-2.0
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

macro (PREFIX_FIND_INCLUDE_DIR prefix includefile libpath_var)
  string (TOUPPER ${prefix}_INCLUDE_DIR tmp_varname)
  find_path(${tmp_varname} ${includefile}
    PATHS ${${libpath_var}}
    PATH_SUFFIXES openjph
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
set (OPENJPH_include_paths
     /usr/include
     /opt/local/include
     /usr/local/include
    )

set (OPENJPH_library_paths
  /usr/lib
  /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
  /usr/local/lib
  /sw/lib
  /opt/local/lib)



# Locate the header files
PREFIX_FIND_INCLUDE_DIR (OPENJPH ojph_arg.h OPENJPH_include_paths)

# If the headers were found, add its parent to the list of lib directories
if (OPENJPH_INCLUDE_DIR)
  get_filename_component (tmp_extra_dir "${OPENJPH_INCLUDE_DIR}/../" ABSOLUTE)
  list (APPEND OPENJPH_library_paths ${tmp_extra_dir})
  unset (tmp_extra_dir)
endif ()

# Search for opj_config.h -- it is only part of OPENJPH >= 2.0, and will
# contain symbols OPJ_VERSION_MAJOR and OPJ_VERSION_MINOR. If the file
# doesn't exist, we're dealing with OPENJPH 1.x.
# Note that for OPENJPH 2.x, the library is named libopenjp2, not
# libOPENJPH (which is for 1.x)
set (OPENJPH_CONFIG_FILE "${OPENJPH_INCLUDE_DIR}/ojph_version.h")
message("!!!!!!!!!!!!!!!!!!OPENJPH OPENJPH_CONFIG_FILE ${OPENJPH_INCLUDE_DIR}   ${OPENJPH_CONFIG_FILE}")

if (EXISTS "${OPENJPH_CONFIG_FILE}")
        message("!!!!!!--- CONFIGFILE - ${OPENJPH_CONFIG_FILE}")
        file(STRINGS "${OPENJPH_CONFIG_FILE}" TMP REGEX "^#define OPENJPH_VERSION_MAJOR .*$")
        string (REGEX MATCHALL "[0-9]+" OJPH_VERSION_MAJOR ${TMP})
        file(STRINGS "${OPENJPH_CONFIG_FILE}" TMP REGEX "^#define OPENJPH_VERSION_MINOR .*$")
        string (REGEX MATCHALL "[0-9]+" OJPH_VERSION_MINOR ${TMP})
endif ()
set (OPENJPH_VERSION "${OJPH_VERSION_MAJOR}.${OJPH_VERSION_MINOR}")
message("!!!!!!!!!!!!!!!!!!OPENJPH VERSION ${OPENJPH_VERSION}")

# Locate the OPENJPH library
set (OPENJPH_libvars "")
set (OPENJPH_cachevars "")

PREFIX_FIND_LIB (OPENJPH openjph
      OPENJPH_library_paths OPENJPH_libvars OPENJPH_cachevars)


# Use the standard function to handle OPENJPH_FOUND
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OPENJPH
  VERSION_VAR OPENJPH_VERSION
  REQUIRED_VARS OPENJPH_INCLUDE_DIR ${OPENJPH_libvars})

if (OPENJPH_FOUND)
  set (OPENJPH_INCLUDES ${OPENJPH_INCLUDE_DIR})
  set (OPENJPH_LIBRARIES "")
  foreach (tmplib ${OPENJPH_libvars})
    list (APPEND OPENJPH_LIBRARIES ${${tmplib}})
  endforeach ()
  if (NOT OPENJPH_FIND_QUIETLY)
    FIND_PACKAGE_MESSAGE (OPENJPH
      "Found OPENJPH: v${OPENJPH_VERSION} ${OPENJPH_LIBRARIES}"
      "[${OPENJPH_INCLUDE_DIR}][${OPENJPH_LIBRARIES}]"
      )
  endif ()
endif ()

unset (OPENJPH_include_paths)
unset (OPENJPH_library_paths)
unset (OPENJPH_libvars)
unset (OPENJPH_cachevars)
