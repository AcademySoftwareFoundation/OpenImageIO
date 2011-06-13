# Module to find OpenJpeg.
#
# This module will first look into the directories defined by the variables:
#   - OPENJPEG_HOME
#
# This module defines the following variables:
#
# OPENJPEG_INCLUDE_DIR - where to find openjpeg.h
# OPENJPEG_LIBRARIES   - list of libraries to link against when using OpenJpeg.
# OPENJPEG_FOUND       - True if OpenJpeg was found.
include (FindPackageHandleStandardArgs)
include (FindPackageMessage)

macro (PREFIX_FIND_INCLUDE_DIR prefix includefile libpath_var)
  string (TOUPPER ${prefix}_INCLUDE_DIR tmp_varname)
  find_path(${tmp_varname} ${includefile}
    PATHS ${${libpath_var}}
    PATH_SUFFIXES include
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
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH
  )
  find_library(${tmp_prefix}_LIBRARY_DEBUG
    NAMES ${libname}d ${libname}_d ${libname}debug ${libname}_debug
    PATHS ${${libpath_var}}
    PATH_SUFFIXES lib
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
     /usr/local/include/openjpeg
     /usr/local/include
     /usr/include/openjpeg
     /usr/include
     /opt/local/include)

set (OpenJpeg_library_paths
  /usr/lib
  /usr/local/lib
  /sw/lib
  /opt/local/lib)

if (OPENJPEG_HOME)
  set (OpenJpeg_library_paths
       ${OpenJpeg_library_paths}
       ${OPENJPEG_HOME}/lib
       ${OPENJPEG_HOME}/lib64)
  set (OpenJpeg_include_paths
       ${OpenJpeg_include_paths}
       ${OPENJPEG_HOME}/include)
endif()



# Locate the header files
PREFIX_FIND_INCLUDE_DIR (OpenJpeg openjpeg.h OpenJpeg_include_paths)

# If the headers were found, add its parent to the list of lib directories
if (OPENJPEG_INCLUDE_DIR)
  get_filename_component (tmp_extra_dir "${OPENJPEG_INCLUDE_DIR}/../" ABSOLUTE)
  list (APPEND OpenJPEG_library_paths ${tmp_extra_dir})
  unset (tmp_extra_dir)
endif ()


# Locate the OpenEXR library
set (OpenJpeg_libvars "")
set (OpenJpeg_cachevars "")
PREFIX_FIND_LIB (OpenJpeg openjpeg
  OpenJpeg_library_paths OpenJpeg_libvars OpenJpeg_cachevars)

# Use the standard function to handle OPENEXR_FOUND
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenJpeg DEFAULT_MSG
  OPENJPEG_INCLUDE_DIR ${OpenJpeg_libvars})

if (OPENJPEG_FOUND)
  set (OPENJPEG_LIBRARIES "")
  foreach (tmplib ${OpenJpeg_libvars})
    list (APPEND OPENJPEG_LIBRARIES ${${tmplib}})
  endforeach ()
  FIND_PACKAGE_MESSAGE (OPENJPEG
    "Found OpenJPEG: ${OPENJPEG_LIBRARIES}"
    "[${OPENJPEG_INCLUDE_DIR}][${OPENJPEG_LIBRARIES}]"
  )
endif ()

unset (OpenJpeg_include_paths)
unset (OpenJpeg_library_paths)
unset (OpenJpeg_libvars)
unset (OpenJpeg_cachevars)
