# Copyright 2008-present Contributors to the OpenImageIO project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/OpenImageIO/oiio/blob/master/LICENSE.md

###########################################################################
# Find external dependencies
###########################################################################

if (NOT VERBOSE)
    set (Boost_FIND_QUIETLY true)
    set (PkgConfig_FIND_QUIETLY true)
    set (Threads_FIND_QUIETLY true)
endif ()

message (STATUS "${ColorBoldWhite}")
message (STATUS "* Checking for dependencies...")
message (STATUS "*   - Missing a dependency 'Package'?")
message (STATUS "*     Try cmake -DPackage_ROOT=path or set environment var Package_ROOT=path")
message (STATUS "*   - To exclude an optional dependency (even if found),")
message (STATUS "*     -DUSE_Package=OFF or set environment var USE_Package=OFF ")
message (STATUS "${ColorReset}")

set (OIIO_LOCAL_DEPS_PATH "${CMAKE_SOURCE_DIR}/ext/dist" CACHE STRING
     "Local area for dependencies added to CMAKE_PREFIX_PATH")
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/ext/dist)

set (REQUIED_DEPS "" CACHE STRING
     "Additional dependencies to consider required (semicolon-separated list, or ALL)")
set (OPTIONAL_DEPS "" CACHE STRING
     "Additional dependencies to consider optional (semicolon-separated list, or ALL)")


# checked_find_package(pkgname ..) is a wrapper for find_package, with the
# following extra features:
#   * If either `USE_pkgname` or the all-uppercase `USE_PKGNAME` (or
#     `ENABLE_pkgname` or `ENABLE_PKGNAME`) exists as either a CMake or
#     environment variable, is nonempty by contains a non-true/nonnzero
#     value, do not search for or use the package. The optional ENABLE <var>
#     arguments allow you to override the name of the enabling variable. In
#     other words, support for the dependency is presumed to be ON, unless
#     turned off explicitly from one of these sources.
#   * Print a message if the package is enabled but not found. This is based
#     on ${pkgname}_FOUND or $PKGNNAME_FOUND.
#   * Optional DEFINITIONS <string> are passed to add_definitions if the
#     package is found.
#   * Optional PRINT <list> is a list of variables that will be printed
#     if the package is found, if VERBOSE is on.
#   * Optional DEPS <list> is a list of hard dependencies; for each one, if
#     dep_FOUND is not true, disable this package with an error message.
#   * Optional ISDEPOF <downstream> names another package for which the
#     present package is only needed because it's a dependency, and
#     therefore if <downstream> is disabled, we don't bother with this
#     package either.
#
# N.B. This needs to be a macro, not a function, because the find modules
# will set(blah val PARENT_SCOPE) and we need that to be the global scope,
# not merely the scope for this function.
macro (checked_find_package pkgname)
    cmake_parse_arguments(_pkg "" "ENABLE;ISDEPOF" "DEFINITIONS;PRINT;DEPS" ${ARGN})
        # Arguments: <prefix> noValueKeywords singleValueKeywords multiValueKeywords argsToParse
    string (TOUPPER ${pkgname} pkgname_upper)
    if (NOT VERBOSE)
        set (${pkgname}_FIND_QUIETLY true)
        set (${pkgname_upper}_FIND_QUIETLY true)
    endif ()
    if ("${pkgname}" IN_LIST REQUIRED_DEPS OR "ALL" IN_LIST REQUIRED_DEPS)
        set (_pkg_REQUIRED 1)
    endif ()
    if ("${pkgname}" IN_LIST OPTIONAL_DEPS OR "ALL" IN_LIST OPTIONAL_DEPS)
        set (_pkg_REQUIRED 0)
    endif ()
    set (_quietskip false)
    check_is_enabled (${pkgname} _enable)
    set (_disablereason "")
    foreach (_dep ${_pkg_DEPS})
        if (_enable AND NOT ${_dep}_FOUND)
            set (_enable false)
            set (ENABLE_${pkgname} OFF PARENT_SCOPE)
            set (_disablereason "(because ${_dep} was not found)")
        endif ()
    endforeach ()
    if (_pkg_ISDEPOF)
        check_is_enabled (${_pkg_ISDEPOF} _dep_enabled)
        if (NOT _dep_enabled)
            set (_enable false)
            set (_quietskip true)
        endif ()
    endif ()
    if (_enable)
        find_package (${pkgname} ${_pkg_UNPARSED_ARGUMENTS})
        if (${pkgname}_FOUND OR ${pkgname_upper}_FOUND)
            foreach (_vervar ${pkgname_upper}_VERSION ${pkgname}_VERSION_STRING
                             ${pkgname_upper}_VERSION_STRING)
                if (NOT ${pkgname}_VERSION AND ${_vervar})
                    set (${pkgname}_VERSION ${${_vervar}})
                endif ()
            endforeach ()
            message (STATUS "${ColorGreen}Found ${pkgname} ${${pkgname}_VERSION} ${ColorReset}")
            if (VERBOSE)
                set (_vars_to_print ${pkgname}_INCLUDES ${pkgname_upper}_INCLUDES
                                    ${pkgname}_INCLUDE_DIR ${pkgname_upper}_INCLUDE_DIR
                                    ${pkgname}_INCLUDE_DIRS ${pkgname_upper}_INCLUDE_DIRS
                                    ${pkgname}_LIBRARIES ${pkgname_upper}_LIBRARIES
                                    ${_pkg_PRINT})
                list (REMOVE_DUPLICATES _vars_to_print)
                foreach (_v IN LISTS _vars_to_print)
                    if (NOT "${${_v}}" STREQUAL "")
                        message (STATUS "    ${_v} = ${${_v}}")
                    endif ()
                endforeach ()
            endif ()
            add_definitions (${_pkg_DEFINITIONS})
        else ()
            message (STATUS "${ColorRed}${pkgname} library not found ${ColorReset}")
            if (${pkgname}_ROOT)
                message (STATUS "${ColorRed}    ${pkgname}_ROOT was: ${${pkgname}_ROOT} ${ColorReset}")
            elseif ($ENV{${pkgname}_ROOT})
                message (STATUS "${ColorRed}    ENV ${pkgname}_ROOT was: ${${pkgname}_ROOT} ${ColorReset}")
            else ()
                message (STATUS "${ColorRed}    Try setting ${pkgname}_ROOT ? ${ColorReset}")
            endif ()
        endif()
    else ()
        if (NOT _quietskip)
            message (STATUS "${ColorRed}Not using ${pkgname} -- disabled ${_disablereason} ${ColorReset}")
        endif ()
    endif ()
endmacro()




include (ExternalProject)

option (BUILD_MISSING_DEPS "Try to download and build any missing dependencies" OFF)


###########################################################################
# Boost setup
if (LINKSTATIC)
    set (Boost_USE_STATIC_LIBS ON)
    if (MSVC)
        add_definitions (-DBOOST_ALL_NO_LIB=1)
    endif ()
else ()
    if (MSVC)
        add_definitions (-DBOOST_ALL_DYN_LINK=1)
    endif ()
endif ()
if (BOOST_CUSTOM)
    set (Boost_FOUND true)
    # N.B. For a custom version, the caller had better set up the variables
    # Boost_VERSION, Boost_INCLUDE_DIRS, Boost_LIBRARY_DIRS, Boost_LIBRARIES.
else ()
    set (Boost_COMPONENTS filesystem system thread)
    if (NOT USE_STD_REGEX)
        list (APPEND Boost_COMPONENTS regex)
    endif ()
    # The FindBoost.cmake interface is broken if it uses boost's installed
    # cmake output (e.g. boost 1.70.0, cmake <= 3.14). Specifically it fails
    # to set the expected variables printed below. So until that's fixed
    # force FindBoost.cmake to use the original brute force path.
    set (Boost_NO_BOOST_CMAKE ON)
    checked_find_package (Boost 1.53 REQUIRED
                       COMPONENTS ${Boost_COMPONENTS}
                       PRINT Boost_INCLUDE_DIRS Boost_LIBRARIES
                      )
endif ()

# On Linux, Boost 1.55 and higher seems to need to link against -lrt
if (CMAKE_SYSTEM_NAME MATCHES "Linux"
      AND ${Boost_VERSION} VERSION_GREATER_EQUAL 105500)
    list (APPEND Boost_LIBRARIES "rt")
endif ()

include_directories (SYSTEM "${Boost_INCLUDE_DIRS}")
link_directories ("${Boost_LIBRARY_DIRS}")

# end Boost setup
###########################################################################


###########################################################################
# Dependencies for required formats and features. These are so critical
# that we will not complete the build if they are not found.

checked_find_package (ZLIB REQUIRED)  # Needed by several packages
checked_find_package (TIFF 3.0 REQUIRED)

# IlmBase & OpenEXR
checked_find_package (OpenEXR 2.0 REQUIRED)
# We use Imath so commonly, may as well include it everywhere.
include_directories ("${OPENEXR_INCLUDES}" "${ILMBASE_INCLUDES}"
                     "${ILMBASE_INCLUDES}/OpenEXR")
if (CMAKE_COMPILER_IS_CLANG AND OPENEXR_VERSION VERSION_LESS 2.3)
    # clang C++ >= 11 doesn't like 'register' keyword in old exr headers
    add_compile_options (-Wno-deprecated-register)
endif ()
if (MSVC AND NOT LINKSTATIC)
    add_definitions (-DOPENEXR_DLL) # Is this needed for new versions?
endif ()


# JPEG -- prefer Turbo-JPEG to regular libjpeg
checked_find_package (JPEGTurbo
                      DEFINITIONS -DUSE_JPEG_TURBO=1
                      PRINT       JPEG_INCLUDES JPEG_LIBRARIES)
if (NOT JPEG_FOUND) # Try to find the non-turbo version
    checked_find_package (JPEG REQUIRED)
endif ()

# Pugixml setup.  Normally we just use the version bundled with oiio, but
# some linux distros are quite particular about having separate packages so we
# allow this to be overridden to use the distro-provided package if desired.
option (USE_EXTERNAL_PUGIXML "Use an externally built shared library version of the pugixml library" OFF)
if (USE_EXTERNAL_PUGIXML)
    checked_find_package (PugiXML REQUIRED
                       DEFINITIONS -DUSE_EXTERNAL_PUGIXML=1)
endif()



###########################################################################
# Dependencies for optional formats and features. If these are not found,
# we will continue building, but the related functionality will be disabled.

checked_find_package (PNG)

checked_find_package (BZip2)   # Used by ffmpeg and freetype
if (NOT BZIP2_FOUND)
    set (BZIP2_LIBRARIES "")  # TODO: why does it break without this?
endif ()

checked_find_package (Freetype
                   DEFINITIONS  -DUSE_FREETYPE=1 )

checked_find_package (HDF5
                   ISDEPOF      Field3D)
checked_find_package (OpenColorIO
                   DEFINITIONS  -DUSE_OCIO=1 -DUSE_OPENCOLORIO=1)
checked_find_package (OpenCV
                   DEFINITIONS  -DUSE_OPENCV=1)

# Intel TBB
set (TBB_USE_DEBUG_BUILD OFF)
checked_find_package (TBB 2017
                   DEFINITIONS  -DUSE_TBB=1
                   ISDEPOF      OpenVDB)

checked_find_package (DCMTK 3.6.1)  # For DICOM images
checked_find_package (FFmpeg 2.6)
checked_find_package (Field3D
                   DEPS         HDF5
                   DEFINITIONS  -DUSE_FIELD3D=1)
checked_find_package (GIF 4)
checked_find_package (Libheif 1.3)  # For HEIF/HEIC format
checked_find_package (LibRaw)
checked_find_package (OpenJpeg)
checked_find_package (OpenVDB 5.0
                   DEPS         TBB
                   DEFINITIONS  -DUSE_OPENVDB=1)
checked_find_package (PTex)
checked_find_package (Webp)

option (USE_R3DSDK "Enable R3DSDK (RED camera) support" OFF)
checked_find_package (R3DSDK)  # RED camera

set (NUKE_VERSION "7.0" CACHE STRING "Nuke version to target")
checked_find_package (Nuke)

checked_find_package (OpenGL)   # used for iv

# Qt -- used for iv
set (qt5_modules Core Gui Widgets)
if (OPENGL_FOUND)
    list (APPEND qt5_modules OpenGL)
endif ()
option (USE_QT "Use Qt if found" ON)
checked_find_package (Qt5 COMPONENTS ${qt5_modules})
if (USE_QT AND NOT Qt5_FOUND AND APPLE)
    message (STATUS "  If you think you installed qt5 with Homebrew and it still doesn't work,")
    message (STATUS "  try:   export PATH=/usr/local/opt/qt5/bin:$PATH")
endif ()




###########################################################################
# Tessil/robin-map

option (BUILD_ROBINMAP_FORCE "Force local download/build of robin-map even if installed" OFF)
option (BUILD_MISSING_ROBINMAP "Local download/build of robin-map if not installed" ON)
set (BUILD_ROBINMAP_VERSION "v0.6.2" CACHE STRING "Preferred Tessil/robin-map version, of downloading/building our own")

macro (find_or_download_robin_map)
    # If we weren't told to force our own download/build of robin-map, look
    # for an installed version. Still prefer a copy that seems to be
    # locally installed in this tree.
    if (NOT BUILD_ROBINMAP_FORCE)
        find_package (Robinmap QUIET)
    endif ()
    # If an external copy wasn't found and we requested that missing
    # packages be built, or we we are forcing a local copy to be built, then
    # download and build it.
    # Download the headers from github
    if ((BUILD_MISSING_ROBINMAP AND NOT ROBINMAP_FOUND) OR BUILD_ROBINMAP_FORCE)
        message (STATUS "Downloading local Tessil/robin-map")
        set (ROBINMAP_INSTALL_DIR "${PROJECT_SOURCE_DIR}/ext/robin-map")
        set (ROBINMAP_GIT_REPOSITORY "https://github.com/Tessil/robin-map")
        if (NOT IS_DIRECTORY ${ROBINMAP_INSTALL_DIR}/include/tsl)
            find_package (Git REQUIRED)
            execute_process(COMMAND             ${GIT_EXECUTABLE} clone    ${ROBINMAP_GIT_REPOSITORY} -n ${ROBINMAP_INSTALL_DIR})
            execute_process(COMMAND             ${GIT_EXECUTABLE} checkout ${BUILD_ROBINMAP_VERSION}
                            WORKING_DIRECTORY   ${ROBINMAP_INSTALL_DIR})
            if (IS_DIRECTORY ${ROBINMAP_INSTALL_DIR}/include/tsl)
                message (STATUS "DOWNLOADED Tessil/robin-map to ${ROBINMAP_INSTALL_DIR}.\n"
                         "Remove that dir to get rid of it.")
            else ()
                message (FATAL_ERROR "Could not download Tessil/robin-map")
            endif ()
        endif ()
        set (ROBINMAP_INCLUDE_DIR "${ROBINMAP_INSTALL_DIR}/include")
    endif ()
    checked_find_package (Robinmap REQUIRED)
endmacro()


###########################################################################
# libsquish

option (USE_EMBEDDED_LIBSQUISH
        "Force use of embedded Libsquish, even if external is found" OFF)
if (NOT USE_EMBEDDED_LIBSQUISH)
    checked_find_package (Libsquish)
endif ()


###########################################################################
# fmtlib

option (BUILD_FMT_FORCE "Force local download/build of fmt even if installed" OFF)
option (BUILD_MISSING_FMT "Local download/build of fmt if not installed" ON)
set (BUILD_FMT_VERSION "6.1.2" CACHE STRING "Preferred fmtlib/fmt version, when downloading/building our own")

macro (find_or_download_fmt)
    # If we weren't told to force our own download/build of fmt, look
    # for an installed version. Still prefer a copy that seems to be
    # locally installed in this tree.
    if (NOT BUILD_FMT_FORCE)
        find_package (fmt QUIET)
    endif ()
    # If an external copy wasn't found and we requested that missing
    # packages be built, or we we are forcing a local copy to be built, then
    # download and build it.
    if ((BUILD_MISSING_FMT AND NOT FMT_FOUND) OR BUILD_FMT_FORCE)
        message (STATUS "Downloading local fmtlib/fmt")
        set (FMT_INSTALL_DIR "${PROJECT_SOURCE_DIR}/ext/fmt")
        set (FMT_GIT_REPOSITORY "https://github.com/fmtlib/fmt")
        if (NOT IS_DIRECTORY ${FMT_INSTALL_DIR}/include/fmt)
            find_package (Git REQUIRED)
            execute_process(COMMAND             ${GIT_EXECUTABLE} clone    ${FMT_GIT_REPOSITORY} -n ${FMT_INSTALL_DIR})
            execute_process(COMMAND             ${GIT_EXECUTABLE} checkout ${BUILD_FMT_VERSION}
                            WORKING_DIRECTORY   ${FMT_INSTALL_DIR})
            if (IS_DIRECTORY ${FMT_INSTALL_DIR}/include/fmt)
                message (STATUS "DOWNLOADED fmtlib/fmt to ${FMT_INSTALL_DIR}.\n"
                         "Remove that dir to get rid of it.")
            else ()
                message (FATAL_ERROR "Could not download fmtlib/fmt")
            endif ()
        endif ()
        set (FMT_INCLUDE_DIR "${FMT_INSTALL_DIR}/include")
    endif ()
    checked_find_package (fmt REQUIRED)
endmacro()

find_or_download_fmt()
include_directories (${FMT_INCLUDES})
