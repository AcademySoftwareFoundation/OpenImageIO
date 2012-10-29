# Module to find OpenEXR.
#
# This module will first look into the directories defined by the variables:
#   - OPENEXR_HOME, OPENEXR_VERSION, OPENEXR_LIB_AREA
#
# It also supports non-standard names for the library components.
#
# To use a custom OpenEXR
#   - Set the variable OPENEXR_CUSTOM to True
#   - Set the variable OPENEXR_CUSTOM_LIBRARY to the name of the library to
#     use, e.g. "SpiIlmImf"
#
# This module defines the following variables:
#
# OPENEXR_INCLUDE_DIR - where to find ImfRgbaFile.h, OpenEXRConfig, etc.
# OPENEXR_LIBRARIES   - list of libraries to link against when using OpenEXR.
#                       This list does NOT include the IlmBase libraries.
#                       These are defined by the FindIlmBase module.
# OPENEXR_FOUND       - True if OpenEXR was found.

# Other standarnd issue macros
include (FindPackageHandleStandardArgs)
include (FindPackageMessage)


# Macro to assemble a helper state variable
macro (SET_STATE_VAR varname)
  set (tmp_lst
    ${OPENEXR_CUSTOM} | ${OPENEXR_CUSTOM_LIBRARY} |
    ${OPENEXR_HOME} | ${OPENEXR_VERSION} | ${OPENEXR_LIB_AREA}
  )
  set (${varname} "${tmp_lst}")
  unset (tmp_lst)
endmacro ()


# Macro to search for an include directory
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


# Macro to search for the given library and adds the cached
# variable names to the specified list
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


# Encode the current state of the external variables into a string
SET_STATE_VAR (OPENEXR_CURRENT_STATE)

# If the state has changed, clear the cached variables
if (OPENEXR_CACHED_STATE AND
    NOT OPENEXR_CACHED_STATE STREQUAL OPENEXR_CURRENT_STATE)
  foreach (libvar ${OPENEXR_CACHED_VARS})
    unset (${libvar} CACHE)
  endforeach ()
endif ()

if (OPENEXR_CUSTOM)
  if (NOT OPENEXR_CUSTOM_LIBRARY)
    message (FATAL_ERROR "Custom OpenEXR library requested but OPENEXR_CUSTOM_LIBRARY is not set.")
  endif()
  set (OpenEXR_Library ${OPENEXR_CUSTOM_LIBRARY})
else ()
  set (OpenEXR_Library IlmImf)
endif ()

# Generic search paths
set (OpenEXR_generic_include_paths
  /usr/include
  /usr/local/include
  /sw/include
  /opt/local/include)
set (OpenEXR_generic_library_paths
  /usr/lib
  /usr/local/lib
  /sw/lib
  /opt/local/lib)

# Search paths for the OpenEXR files
if (OPENEXR_HOME)
  set (OpenEXR_library_paths
    ${OPENEXR_HOME}/lib
    ${OPENEXR_HOME}/lib64)
  if (OPENEXR_VERSION)
    set (OpenEXR_include_paths
      ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/include
      ${OPENEXR_HOME}/include/openexr-${OPENEXR_VERSION})
    list (APPEND OpenEXR_library_paths
      ${OPENEXR_HOME}/openexr-${OPENEXR_VERSION}/lib
      ${OPENEXR_HOME}/lib/openexr-${OPENEXR_VERSION})
  endif()
  list (APPEND OpenEXR_include_paths ${OPENEXR_HOME}/include)
  if (OPENEXR_LIB_AREA)
    list (INSERT OpenEXR_library_paths 2 ${OPENEXR_LIB_AREA})
  endif ()
endif ()
if (ILMBASE_HOME AND OPENEXR_VERSION)
  list (APPEND OpenEXR_include_paths
    ${ILMBASE_HOME}/include/openexr-${OPENEXR_VERSION})
endif()
list (APPEND OpenEXR_include_paths ${OpenEXR_generic_include_paths})
list (APPEND OpenEXR_library_paths ${OpenEXR_generic_library_paths})

# Locate the header files
PREFIX_FIND_INCLUDE_DIR (OpenEXR
  OpenEXR/ImfArray.h OpenEXR_include_paths)

# If the headers were found, add its parent to the list of lib directories
if (OPENEXR_INCLUDE_DIR)
  get_filename_component (tmp_extra_dir "${OPENEXR_INCLUDE_DIR}/../" ABSOLUTE)
  list (APPEND OpenEXR_library_paths ${tmp_extra_dir})
  unset (tmp_extra_dir)
endif ()

# Locate the OpenEXR library
set (OpenEXR_libvars "")
set (OpenEXR_cachevars "")
PREFIX_FIND_LIB (OpenEXR ${OpenEXR_Library}
  OpenEXR_library_paths OpenEXR_libvars OpenEXR_cachevars)

# Create the list of variables that might need to be cleared
set (OPENEXR_CACHED_VARS
  OPENEXR_INCLUDE_DIR ${OpenEXR_cachevars}
  CACHE INTERNAL "Variables set by FindOpenEXR.cmake" FORCE)

# Store the current state so that variables might be cleared if required
set (OPENEXR_CACHED_STATE ${OPENEXR_CURRENT_STATE}
  CACHE INTERNAL "State last seen by FindOpenEXR.cmake" FORCE)

# Always link explicitly with zlib
set (OPENEXR_ZLIB ${ZLIB_LIBRARIES})

# Use the standard function to handle OPENEXR_FOUND
FIND_PACKAGE_HANDLE_STANDARD_ARGS (OpenEXR DEFAULT_MSG
  OPENEXR_INCLUDE_DIR ${OpenEXR_libvars})

if (OPENEXR_FOUND)
  set (OPENEXR_LIBRARIES "")
  foreach (tmplib ${OpenEXR_libvars})
    list (APPEND OPENEXR_LIBRARIES ${${tmplib}})
  endforeach ()
  list (APPEND OPENEXR_LIBRARIES ${ZLIB_LIBRARIES})
  if (VERBOSE)
    FIND_PACKAGE_MESSAGE (OPENEXR
      "Found OpenEXR: ${OPENEXR_LIBRARIES}"
      "[${OPENEXR_INCLUDE_DIR}][${OPENEXR_LIBRARIES}][${OPENEXR_CURRENT_STATE}]"
      )
  endif ()
endif ()

# Unset the helper variables to avoid pollution
unset (OPENEXR_CURRENT_STATE)
unset (OpenEXR_include_paths)
unset (OpenEXR_library_paths)
unset (OpenEXR_generic_include_paths)
unset (OpenEXR_generic_library_paths)
unset (OpenEXR_libvars)
unset (OpenEXR_cachevars)
