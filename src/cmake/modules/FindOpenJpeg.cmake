# Module to find OpenJpeg.
#
# This module will first look into the directories defined by the variables:
#   - OpenJpeg_ROOT
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

macro (PREFIX_FIND_INCLUDE_DIR prefix includefile libpath_var)
  string (TOUPPER ${prefix}_INCLUDE_DIR tmp_varname)
  find_path(${tmp_varname} ${includefile}
    PATHS ${${libpath_var}}
    NO_DEFAULT_PATH
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
    NO_DEFAULT_PATH
  )
  find_library(${tmp_prefix}_LIBRARY_DEBUG
    NAMES ${libname}d ${libname}_d ${libname}debug ${libname}_debug
    PATHS ${${libpath_var}}
    NO_DEFAULT_PATH
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
     /usr/local/include/openjpeg-2.3
     /usr/local/include/openjpeg-2.2
     /usr/local/include/openjpeg-2.1
     /usr/local/include/openjpeg-2.0
     /usr/local/include/openjpeg
     /usr/local/include
     /usr/include/openjpeg-2.3
     /usr/include/openjpeg-2.2
     /usr/include/openjpeg-2.1
     /usr/include/openjpeg
     /usr/include
     /opt/local/include
     /opt/local/include/openjpeg-2.3
     /opt/local/include/openjpeg-2.2
     /opt/local/include/openjpeg-2.1
     /opt/local/include/openjpeg-2.0)

set (OpenJpeg_library_paths
  /usr/lib
  /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
  /usr/local/lib
  /sw/lib
  /opt/local/lib)

if (OpenJpeg_ROOT)
  set (OpenJpeg_library_paths
       ${OpenJpeg_ROOT}/lib
       ${OpenJpeg_ROOT}/lib64
       ${OpenJpeg_ROOT}/bin
       ${OpenJpeg_library_paths}
      )
  set (OpenJpeg_include_paths
       ${OpenJpeg_ROOT}/include/openjpeg-2.3
       ${OpenJpeg_ROOT}/include/openjpeg-2.2
       ${OpenJpeg_ROOT}/include/openjpeg-2.1
       ${OpenJpeg_ROOT}/include/openjpeg-2.0
       ${OpenJpeg_ROOT}/include/openjpeg
       ${OpenJpeg_ROOT}/include
       ${OpenJpeg_include_paths}
      )
endif()


# Locate the header files
PREFIX_FIND_INCLUDE_DIR (OpenJpeg openjpeg.h OpenJpeg_include_paths)

# If the headers were found, add its parent to the list of lib directories
if (OPENJPEG_INCLUDE_DIR)
  get_filename_component (tmp_extra_dir "${OPENJPEG_INCLUDE_DIR}/../" ABSOLUTE)
  list (APPEND OpenJPEG_library_paths ${tmp_extra_dir})
  unset (tmp_extra_dir)
endif ()

# Search for opj_config.h -- it is only part of OpenJpeg >= 2.0, and will
# contain symbols OPJ_VERSION_MAJOR and OPJ_VERSION_MINOR. If the file
# doesn't exist, we're dealing with OpenJPEG 1.x.
# Note that for OpenJPEG 2.x, the library is named libopenjp2, not
# libopenjpeg (which is for 1.x)
set (OPENJPEG_CONFIG_FILE "${OPENJPEG_INCLUDE_DIR}/opj_config.h")
if (EXISTS "${OPENJPEG_CONFIG_FILE}")
    file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_PACKAGE_VERSION .*$")
    if (TMP)
        # 2.0 is the only one with this construct
        set (OPJ_VERSION_MAJOR 2)
        set (OPJ_VERSION_MINOR 0)
    else ()
        # 2.1 and beyond
        file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_VERSION_MAJOR .*$")
        string (REGEX MATCHALL "[0-9]+" OPJ_VERSION_MAJOR ${TMP})
        file(STRINGS "${OPENJPEG_CONFIG_FILE}" TMP REGEX "^#define OPJ_VERSION_MINOR .*$")
        string (REGEX MATCHALL "[0-9]+" OPJ_VERSION_MINOR ${TMP})
    endif ()
else ()
    # Guess OpenJPEG 1.5 -- older versions didn't have the version readily
    # apparent in the headers.
    set (OPJ_VERSION_MAJOR 1)
    set (OPJ_VERSION_MINOR 5)
endif ()
set (OPENJPEG_VERSION "${OPJ_VERSION_MAJOR}.${OPJ_VERSION_MINOR}")


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
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenJpeg
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
