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
message (STATUS "*     For many dependencies, we supply src/build-scripts/build_Package.bash")
message (STATUS "*   - To exclude an optional dependency (even if found),")
message (STATUS "*     -DUSE_Package=OFF or set environment var USE_Package=OFF ")
message (STATUS "${ColorReset}")

set (OIIO_LOCAL_DEPS_PATH "${CMAKE_SOURCE_DIR}/ext/dist" CACHE STRING
     "Local area for dependencies added to CMAKE_PREFIX_PATH")
list (APPEND CMAKE_PREFIX_PATH ${CMAKE_SOURCE_DIR}/ext/dist)

include (ExternalProject)

option (BUILD_MISSING_DEPS "Try to download and build any missing dependencies" OFF)


###########################################################################
# Boost setup
if (MSVC)
    # Disable automatic linking using pragma comment(lib,...) of boost libraries upon including of a header
    add_definitions (-DBOOST_ALL_NO_LIB=1)
endif ()
if (LINKSTATIC)
    set (Boost_USE_STATIC_LIBS ON)
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
    checked_find_package (Boost REQUIRED
                          VERSION_MIN 1.53
                          COMPONENTS ${Boost_COMPONENTS}
                          RECOMMEND_MIN 1.66
                          RECOMMEND_MIN_REASON "Boost 1.66 is the oldest version our CI tests against"
                          PRINT Boost_INCLUDE_DIRS Boost_LIBRARIES )
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
checked_find_package (TIFF REQUIRED
                      VERSION_MIN 3.9
                      RECOMMEND_MIN 4.0
                      RECOMMEND_MIN_REASON "to support >4GB files")

# IlmBase & OpenEXR
checked_find_package (OpenEXR REQUIRED
                      VERSION_MIN 2.0
                      RECOMMEND_MIN 2.2
                      RECOMMEND_MIN_REASON "for DWA compression"
                      PRINT IMATH_INCLUDES OPENEXR_INCLUDES)
# Force Imath includes to be before everything else to ensure that we have
# the right Imath/OpenEXR version, not some older version in the system
# library. This shoudn't be necessary, except for the common case of people
# building against Imath/OpenEXR 3.x when there is still a system-level
# install version of 2.x.
include_directories(BEFORE ${IMATH_INCLUDES} ${OPENEXR_INCLUDES})
if (CMAKE_COMPILER_IS_CLANG AND OPENEXR_VERSION VERSION_LESS 2.3)
    # clang C++ >= 11 doesn't like 'register' keyword in old exr headers
    add_compile_options (-Wno-deprecated-register)
endif ()
if (MSVC AND NOT LINKSTATIC)
    add_definitions (-DOPENEXR_DLL) # Is this needed for new versions?
endif ()

if (OPENEXR_VERSION VERSION_GREATER_EQUAL 2.5.99)
    set (OIIO_USING_IMATH 3)
else ()
    set (OIIO_USING_IMATH 2)
endif ()


# JPEG -- prefer Turbo-JPEG to regular libjpeg
checked_find_package (JPEGTurbo
                      DEFINITIONS -DUSE_JPEG_TURBO=1
                      PRINT       JPEG_INCLUDES JPEG_INCLUDE_DIRS
                                  JPEG_LIBRARIES)
if (NOT JPEG_FOUND) # Try to find the non-turbo version
    checked_find_package (JPEG REQUIRED)
endif ()

# Pugixml setup.  Normally we just use the version bundled with oiio, but
# some linux distros are quite particular about having separate packages so we
# allow this to be overridden to use the distro-provided package if desired.
option (USE_EXTERNAL_PUGIXML "Use an externally built shared library version of the pugixml library" OFF)
if (USE_EXTERNAL_PUGIXML)
    checked_find_package (pugixml REQUIRED
                          VERSION_MIN 1.8
                          DEFINITIONS -DUSE_EXTERNAL_PUGIXML=1)
else ()
    message (STATUS "Using internal PugiXML")
endif()

# From pythonutils.cmake
find_python()


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

checked_find_package (DCMTK VERSION_MIN 3.6.1)  # For DICOM images
checked_find_package (FFmpeg VERSION_MIN 2.6)
checked_find_package (Field3D
                   DEPS         HDF5
                   DEFINITIONS  -DUSE_FIELD3D=1)
checked_find_package (GIF
                      VERSION_MIN 4
                      RECOMMEND_MIN 5.0
                      RECOMMEND_MIN_REASON "for stability and thread safety")

# For HEIF/HEIC/AVIF formats
checked_find_package (Libheif VERSION_MIN 1.3
                      RECOMMEND_MIN 1.7
                      RECOMMEND_MIN_REASON "for AVIF support")
if (APPLE AND LIBHEIF_VERSION VERSION_GREATER_EQUAL 1.10 AND LIBHEIF_VERSION VERSION_LESS 1.11)
    message (WARNING "Libheif 1.10 on Apple is known to be broken, disabling libheif support")
    set (Libheif_FOUND 0)
endif ()

checked_find_package (LibRaw
                      RECOMMEND_MIN 0.18
                      RECOMMEND_MIN_REASON "for ACES support and better camera metadata"
                      PRINT LibRaw_r_LIBRARIES )
checked_find_package (OpenJpeg VERSION_MIN 2.0)

checked_find_package (OpenVDB
                      VERSION_MIN 5.0
                      DEPS         TBB
                      DEFINITIONS  -DUSE_OPENVDB=1)
if (OpenVDB_FOUND AND OpenVDB_VERSION VERSION_GREATER_EQUAL 8.0
        AND CMAKE_CXX_STANDARD VERSION_LESS 14)
    set (OpenVDB_FOUND OFF)
    add_definitions (-UUSE_OPENVDB)
    message (WARNING
             "${ColorYellow}OpenVDB 8.0+ requires C++14 or higher (was ${CMAKE_CXX_STANDARD}). "
             "To build against this OpenVDB ${OpenVDB_VERSION}, you need to set "
             "build option CMAKE_CXX_STANDARD=14 (or higher). The minimum requirements "
             "for that are gcc >= 5.1, clang >= 3.5, Apple clang >= 7, icc >= 7, MSVS >= 2017. "
             "If you must use C++11, you need to build against OpenVDB 7 or earlier. ${ColorReset}")
    message (STATUS "${ColorRed}Not using OpenVDB -- OpenVDB ${OpenVDB_VERSION} requires C++14 or later. ${ColorReset}")
endif ()

checked_find_package (PTex)
checked_find_package (WebP)

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
set (BUILD_FMT_VERSION "7.1.3" CACHE STRING "Preferred fmtlib/fmt version, when downloading/building our own")

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
