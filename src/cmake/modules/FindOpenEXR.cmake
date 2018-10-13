# Module to find OpenEXR.
#
# This module will set
#   OPENEXR_FOUND          true, if found
#   OPENEXR_INCLUDE_DIR    directory where headers are found
#   OPENEXR_LIBRARIES      libraries for OpenEXR + IlmBase
#   ILMBASE_LIBRARIES      libraries just IlmBase
#   OPENEXR_VERSION        OpenEXR version (accurate for >= 2.0.0,
#                              otherwise will just guess 1.6.1)
#
#

# Other standard issue macros
include (FindPackageHandleStandardArgs)
include (SelectLibraryConfigurations)

find_package (ZLIB REQUIRED)

# Link with pthreads if required
find_package (Threads)
if (CMAKE_USE_PTHREADS_INIT)
    set (ILMBASE_PTHREADS ${CMAKE_THREAD_LIBS_INIT})
endif ()

# Attempt to find OpenEXR with pkgconfig
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(_ILMBASE QUIET IlmBase>=2.0.0)
    pkg_check_modules(_OPENEXR QUIET OpenEXR>=2.0.0)
endif (PKG_CONFIG_FOUND)

# List of likely places to find the headers -- note priority override of
# ${OPENEXR_ROOT_DIR}/include.
# ILMBASE is needed in case ilmbase an openexr are installed in separate
# directories, like NixOS does
set (GENERIC_INCLUDE_PATHS
    ${OPENEXR_ROOT_DIR}/include
    $ENV{OPENEXR_ROOT_DIR}/include
    ${ILMBASE_ROOT_DIR}/include
    $ENV{ILMBASE_ROOT_DIR}/include
    ${_ILMBASE_INCLUDEDIR}
    ${_OPENEXR_INCLUDEDIR}
    /usr/local/include
    /usr/include
    /usr/include/${CMAKE_LIBRARY_ARCHITECTURE}
    /sw/include
    /opt/local/include )

# Find the include file locations. We call find_path twice -- first using
# only the custom paths, then if that fails, try the default paths only.
# This seems to be the most robust way I can find to not get confused when
# both system and custom libraries are present.
find_path (ILMBASE_INCLUDE_PATH OpenEXR/IlmBaseConfig.h
           PATHS ${ILMBASE_INCLUDE_DIR} ${OPENEXR_INCLUDE_DIR}
                 ${GENERIC_INCLUDE_PATHS} NO_DEFAULT_PATH)
find_path (ILMBASE_INCLUDE_PATH OpenEXR/IlmBaseConfig.h)
find_path (OPENEXR_INCLUDE_PATH OpenEXR/OpenEXRConfig.h
           PATHS ${OPENEXR_INCLUDE_DIR}
                 ${GENERIC_INCLUDE_PATHS} NO_DEFAULT_PATH)
find_path (OPENEXR_INCLUDE_PATH OpenEXR/OpenEXRConfig.h)

# Try to figure out version number
if (EXISTS "${OPENEXR_INCLUDE_PATH}/OpenEXR/ImfMultiPartInputFile.h")
    # Must be at least 2.0
    file(STRINGS "${OPENEXR_INCLUDE_PATH}/OpenEXR/OpenEXRConfig.h" TMP REGEX "^#define OPENEXR_VERSION_STRING .*$")
    string (REGEX MATCHALL "[0-9]+[.0-9]+" OPENEXR_VERSION ${TMP})
    file(STRINGS "${OPENEXR_INCLUDE_PATH}/OpenEXR/OpenEXRConfig.h" TMP REGEX "^#define OPENEXR_VERSION_MAJOR .*$")
    string (REGEX MATCHALL "[0-9]+" OPENEXR_VERSION_MAJOR ${TMP})
    file(STRINGS "${OPENEXR_INCLUDE_PATH}/OpenEXR/OpenEXRConfig.h" TMP REGEX "^#define OPENEXR_VERSION_MINOR .*$")
    string (REGEX MATCHALL "[0-9]+" OPENEXR_VERSION_MINOR ${TMP})
else ()
    # Assume an old one, predates 2.x that had versions
    set (OPENEXR_VERSION 1.6.1)
    set (OPENEXR_MAJOR 1)
    set (OPENEXR_MINOR 6)
endif ()


# List of likely places to find the libraries -- note priority override of
# ${OPENEXR_ROOT_DIR}/lib.
set (GENERIC_LIBRARY_PATHS
    ${OPENEXR_ROOT_DIR}/lib
    ${ILMBASE_ROOT_DIR}/lib
    ${OPENEXR_INCLUDE_PATH}/../lib
    ${ILMBASE_INCLUDE_PATH}/../lib
    ${_ILMBASE_LIBDIR}
    ${_OPENEXR_LIBDIR}
    /usr/local/lib
    /usr/local/lib/${CMAKE_LIBRARY_ARCHITECTURE}
    /usr/lib
    /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
    /sw/lib
    /opt/local/lib
    $ENV{PROGRAM_FILES}/OpenEXR/lib/static )

# Handle request for static libs by altering CMAKE_FIND_LIBRARY_SUFFIXES.
# We will restore it at the end of this file.
set (_openexr_orig_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
if (OpenEXR_USE_STATIC_LIBS)
    if (WIN32)
        set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    else ()
        set (CMAKE_FIND_LIBRARY_SUFFIXES .a)
    endif ()
endif ()

# Look for the libraries themselves, for all the components. Like with the
# headers, we do two finds -- first for custom locations, then for default.
# This is complicated because the OpenEXR libraries may or may not be
# built with version numbers embedded.
set (_openexr_components IlmThread IlmImf Imath Iex Half)
foreach (COMPONENT ${_openexr_components})
    string (TOUPPER ${COMPONENT} UPPERCOMPONENT)
    # First try with the version embedded
    set (FULL_COMPONENT_NAME ${COMPONENT}-${OPENEXR_VERSION_MAJOR}_${OPENEXR_VERSION_MINOR})
    find_library (OPENEXR_${UPPERCOMPONENT}_LIBRARY ${FULL_COMPONENT_NAME}
                  PATHS ${OPENEXR_LIBRARY_DIR}
                        ${GENERIC_LIBRARY_PATHS} NO_DEFAULT_PATH)
    # Again, with no directory restrictions
    find_library (OPENEXR_${UPPERCOMPONENT}_LIBRARY ${FULL_COMPONENT_NAME})
    # Try again without the version
    set (FULL_COMPONENT_NAME ${COMPONENT})
    find_library (OPENEXR_${UPPERCOMPONENT}_LIBRARY ${FULL_COMPONENT_NAME}
                  PATHS ${OPENEXR_LIBRARY_DIR}
                        ${GENERIC_LIBRARY_PATHS} NO_DEFAULT_PATH)
    # One more time, with no restrictions
    find_library (OPENEXR_${UPPERCOMPONENT}_LIBRARY ${FULL_COMPONENT_NAME})
endforeach ()

# Set the FOUND, INCLUDE_DIR, and LIBRARIES variables.
if (ILMBASE_INCLUDE_PATH AND OPENEXR_INCLUDE_PATH AND
      OPENEXR_IMATH_LIBRARY AND OPENEXR_ILMIMF_LIBRARY AND
      OPENEXR_IEX_LIBRARY AND OPENEXR_HALF_LIBRARY)
    set (OPENEXR_FOUND TRUE)
    set (ILMBASE_FOUND TRUE)
    set (ILMBASE_INCLUDE_DIR ${ILMBASE_INCLUDE_PATH} CACHE STRING "The include paths needed to use IlmBase")
    set (OPENEXR_INCLUDE_DIR ${OPENEXR_INCLUDE_PATH} CACHE STRING "The include paths needed to use OpenEXR")
    set (ILMBASE_LIBRARIES ${OPENEXR_IMATH_LIBRARY} ${OPENEXR_IEX_LIBRARY} ${OPENEXR_HALF_LIBRARY} ${OPENEXR_ILMTHREAD_LIBRARY} ${ILMBASE_PTHREADS} CACHE STRING "The libraries needed to use IlmBase")
    set (OPENEXR_LIBRARIES ${OPENEXR_ILMIMF_LIBRARY} ${ILMBASE_LIBRARIES} ${ZLIB_LIBRARIES} CACHE STRING "The libraries needed to use OpenEXR")
endif ()

find_package_handle_standard_args (OpenEXR
    REQUIRED_VARS ILMBASE_INCLUDE_PATH OPENEXR_INCLUDE_PATH
                  OPENEXR_IMATH_LIBRARY OPENEXR_ILMIMF_LIBRARY
                  OPENEXR_IEX_LIBRARY OPENEXR_HALF_LIBRARY
    VERSION_VAR   OPENEXR_VERSION
    )

MARK_AS_ADVANCED(
    ILMBASE_INCLUDE_DIR
    OPENEXR_INCLUDE_DIR
    ILMBASE_LIBRARIES
    OPENEXR_LIBRARIES
    OPENEXR_ILMIMF_LIBRARY
    OPENEXR_IMATH_LIBRARY
    OPENEXR_IEX_LIBRARY
    OPENEXR_HALF_LIBRARY
    OPENEXR_VERSION)

# Restore the original CMAKE_FIND_LIBRARY_SUFFIXES
set (CMAKE_FIND_LIBRARY_SUFFIXES ${_openexr_orig_suffixes})
